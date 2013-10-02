/*! @file graphics.c
 *  @copyright Copyright (c) 2013 Kyle Weicht. All rights reserved.
 */
#include "graphics.h"
#include <stdlib.h>
#include "assert.h"
#include "gl_include.h"
#include "program.h"
#include "vertex.h"
#include "forward.h"

/* Defines
 */
#define MAX_RENDER_COMMANDS 1024

/* Types
 */
struct Graphics
{
    int width;
    int height;

    ForwardRenderer*    forward;

    GLint   default_framebuffer;

    GLuint  fullscreen_program;
    GLuint  fullscreen_quad_vertex_buffer;
    GLuint  fullscreen_quad_index_buffer;
    GLuint  fullscreen_texture;

    GLuint  framebuffer;
    GLuint  color_texture;
    GLuint  depth_texture;

    Mat4    proj_matrix;
    Mat4    view_matrix;

    Model   render_commands[MAX_RENDER_COMMANDS];
    int     num_render_commands;
};

/* Constants
 */
static const struct {
    float pos[2];
    float tex[2];
} kFullscreenVertices[] = {
    { {  1.0f,  1.0f }, { 1.0f, 1.0f } },
    { { -1.0f,  1.0f }, { 0.0f, 1.0f } },
    { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
    { {  1.0f, -1.0f }, { 1.0f, 0.0f } },
};
static const uint32_t kFullscreenIndices[] = {
    0, 2, 1,
    0, 3, 2,
};

/* Variables
 */

/* Internal functions
 */
static void _create_fullscreen_quad(Graphics* G)
{
    G->fullscreen_program = create_program("fullscreen_vertex.glsl", "fullscreen_fragment.glsl");
    ASSERT_GL(glUseProgram(G->fullscreen_program));
    G->fullscreen_texture = glGetUniformLocation(G->fullscreen_program, "s_Texture");
    ASSERT_GL(glBindAttribLocation(G->fullscreen_program , kPositionSlot, "a_Position"));
    ASSERT_GL(glBindAttribLocation(G->fullscreen_program , kTexCoordSlot, "a_TexCoord"));
    ASSERT_GL(glEnableVertexAttribArray(kPositionSlot));
    ASSERT_GL(glEnableVertexAttribArray(kTexCoordSlot));
    ASSERT_GL(glUseProgram(0));

    /* Create vertex buffer */
    ASSERT_GL(glGenBuffers(1, &G->fullscreen_quad_vertex_buffer));
    ASSERT_GL(glBindBuffer(GL_ARRAY_BUFFER, G->fullscreen_quad_vertex_buffer));
    ASSERT_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(kFullscreenVertices), kFullscreenVertices, GL_STATIC_DRAW));
    ASSERT_GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    /* Create index buffer */
    ASSERT_GL(glGenBuffers(1, &G->fullscreen_quad_index_buffer));
    ASSERT_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, G->fullscreen_quad_index_buffer));
    ASSERT_GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kFullscreenIndices), kFullscreenIndices, GL_STATIC_DRAW));
    ASSERT_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}
static void _draw_fullscreen_quad(Graphics* G)
{
    float* ptr = 0;
    ASSERT_GL(glBindBuffer(GL_ARRAY_BUFFER, G->fullscreen_quad_vertex_buffer));
    ASSERT_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, G->fullscreen_quad_index_buffer));
    ASSERT_GL(glVertexAttribPointer(kPositionSlot,    2, GL_FLOAT, GL_FALSE, sizeof(kFullscreenVertices[0]), (void*)(ptr+=0)));
    ASSERT_GL(glVertexAttribPointer(kTexCoordSlot,    2, GL_FLOAT, GL_FALSE, sizeof(kFullscreenVertices[0]), (void*)(ptr+=2)));
    ASSERT_GL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL));
}
static void _create_framebuffer(Graphics* G)
{
    /* Color buffer */
    ASSERT_GL(glGenTextures(1, &G->color_texture));
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, G->color_texture));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    /* Depth buffer */
    ASSERT_GL(glGenTextures(1, &G->depth_texture));
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, G->depth_texture));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    ASSERT_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    /* Framebuffer */
    ASSERT_GL(glGenFramebuffers(1, &G->framebuffer));

    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, 0));
}
static void _resize_framebuffer(Graphics* G)
{
    GLenum framebuffer_status;

    /* Color buffer */
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, G->color_texture));
    ASSERT_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, G->width, G->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));

    /* Depth buffer */
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, G->depth_texture));
    ASSERT_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, G->width, G->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0));

    /* Framebuffer */
    ASSERT_GL(glBindFramebuffer(GL_FRAMEBUFFER, G->framebuffer));
    ASSERT_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, G->color_texture, 0));
    ASSERT_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, G->depth_texture, 0));

    framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
        system_log("Framebuffer error: %s\n", _glStatusString(framebuffer_status));
        assert(0);
    }

    ASSERT_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, 0));
}

/* External functions
 */
Graphics* create_graphics(void)
{
    Graphics* G = NULL;

    /* Allocate graphics */
    G = (Graphics*)calloc(1, sizeof(Graphics));
    G->width = 1;
    G->height = 1;

    /* Set up OpenGL */
    ASSERT_GL(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
    ASSERT_GL(glClearDepthf(1.0f));
    ASSERT_GL(glEnable(GL_DEPTH_TEST));
    //ASSERT_GL(glEnable(GL_CULL_FACE));
    //ASSERT_GL(glFrontFace(GL_CW));
    system_log("OpenGL version:\t%s\n", glGetString(GL_VERSION));
    system_log("OpenGL renderer:\t%s\n", glGetString(GL_RENDERER));

    /* Set up self */
    _create_fullscreen_quad(G);
    _create_framebuffer(G);

    /* Set up renderers */
    G->forward = create_forward_renderer(G);

    return G;
}
void destroy_graphics(Graphics* G)
{
    destroy_forward_renderer(G->forward);
    destroy_program(G->fullscreen_program);
    free(G);
}
void resize_graphics(Graphics* G, int width, int height)
{
    G->width = width;
    G->height = height;
    G->proj_matrix = mat4_perspective_fov(kPiDiv2, width/(float)height, 1.0f, 1000.0f);

    ASSERT_GL(glViewport(0, 0, width, height));
    ASSERT_GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &G->default_framebuffer));

    _resize_framebuffer(G);

    system_log("Graphics resized: %d, %d\n", width, height);
}
void render_graphics(Graphics* G)
{
    ASSERT_GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &G->default_framebuffer));
    //system_log("Default framebuffer: %d\n", G->default_framebuffer);
    /* Bind framebuffer */
    ASSERT_GL(glBindFramebuffer(GL_FRAMEBUFFER, G->framebuffer));
    ASSERT_GL(glViewport(0, 0, G->width, G->height));
    ASSERT_GL(glClearColor(0.3f, 0.6f, 0.9f, 1.0f));
    ASSERT_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    render_forward(G->forward, G->proj_matrix, G->view_matrix, G->render_commands, G->num_render_commands);
    G->num_render_commands = 0;

    /* Bind default framebuffer and render to the screen */
    ASSERT_GL(glBindFramebuffer(GL_FRAMEBUFFER, G->default_framebuffer));
    ASSERT_GL(glViewport(0, 0, G->width, G->height));
    ASSERT_GL(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
    ASSERT_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    ASSERT_GL(glUseProgram(G->fullscreen_program));
    ASSERT_GL(glActiveTexture(GL_TEXTURE0));
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, G->color_texture));
    _draw_fullscreen_quad(G);
    ASSERT_GL(glBindTexture(GL_TEXTURE_2D, 0));
}

void set_view_matrix(Graphics* G, Mat4 view)
{
    G->view_matrix = view;
}
void add_render_command(Graphics* G, Model model)
{
    int index = G->num_render_commands++;
    assert(index <= MAX_RENDER_COMMANDS);
    G->render_commands[index] = model;
}
