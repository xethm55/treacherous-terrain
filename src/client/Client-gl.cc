
#include <math.h>
#include <iostream>
#include GL_INCLUDE_FILE
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include "Client.h"
#include "ccfs.h"
#include "HexTile.h"

using namespace std;

#define OPENGL_CONTEXT_MAJOR 3
#define OPENGL_CONTEXT_MINOR 0
#define NUM_OBJ_UNIFORMS 7

static GLint obj_uniform_locations[NUM_OBJ_UNIFORMS];
static const char *obj_uniform_names[] = {
#define OBJ_AMBIENT obj_uniform_locations[0]
    "ambient",
#define OBJ_DIFFUSE obj_uniform_locations[1]
    "diffuse",
#define OBJ_SPECULAR obj_uniform_locations[2]
    "specular",
#define OBJ_SHININESS obj_uniform_locations[3]
    "shininess",
#define OBJ_SCALE obj_uniform_locations[4]
    "scale",
#define OBJ_PROJECTION obj_uniform_locations[5]
    "projection",
#define OBJ_MODELVIEW obj_uniform_locations[6]
    "modelview"
};
/* points of a horizontal hexagon 1.0 units high */
static const float overlay_hex_attributes[][2] = {
    {0.0, 0.0},
    {HEX_WIDTH_TO_HEIGHT / 2.0, 0.0},
    {HEX_WIDTH_TO_HEIGHT / 4.0, 0.5},
    {-HEX_WIDTH_TO_HEIGHT / 4.0, 0.5},
    {-HEX_WIDTH_TO_HEIGHT / 2.0, 0.0},
    {-HEX_WIDTH_TO_HEIGHT / 4.0, -0.5},
    {HEX_WIDTH_TO_HEIGHT / 4.0, -0.5}
};
static const GLshort overlay_hex_indices[] = {
    0, 1, 2, 3, 4, 5, 6
};

static bool load_file(const char *fname, WFObj::Buffer & buff)
{
    unsigned int length;
    uint8_t *contents = (uint8_t *) CFS.get_file(fname, &length);
    if (contents != NULL)
    {
        buff.data = contents;
        buff.length = length;
        return true;
    }
    return false;
}

bool Client::create_window(bool fullscreen, int width, int height)
{
    sf::VideoMode mode = fullscreen
        ? sf::VideoMode::getDesktopMode()
        : sf::VideoMode(width, height, 32);
    long style = fullscreen
        ? sf::Style::Fullscreen
        : sf::Style::Resize | sf::Style::Close;
    sf::ContextSettings cs = sf::ContextSettings(0, 0, 0,
            OPENGL_CONTEXT_MAJOR, OPENGL_CONTEXT_MINOR);
    m_window = new sf::Window(mode, "Treacherous Terrain", style, cs);
    m_window->setMouseCursorVisible(false);
    if (!initgl())
        return false;
    resize_window(m_window->getSize().x, m_window->getSize().y);
    return true;
}

bool Client::initgl()
{
    if (gl3wInit())
    {
        cerr << "Failed to initialize GL3W" << endl;
        return false;
    }
    if (!gl3wIsSupported(3, 0))
    {
        cerr << "OpenGL 3.0 is not supported!" << endl;
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    GLProgram::AttributeBinding obj_attrib_bindings[] = {
        {0, "pos"},
        {1, "normal"},
        {0, NULL}
    };
    const char *v_source = (const char *) CFS.get_file("shaders/obj_v.glsl", NULL);
    const char *f_source = (const char *) CFS.get_file("shaders/obj_f.glsl", NULL);
    if (v_source == NULL || f_source == NULL)
    {
        cerr << "Error loading shader sources" << endl;
        return false;
    }
    else if (!m_obj_program.create(v_source, f_source, obj_attrib_bindings))
    {
        cerr << "Error creating obj program" << endl;
        return false;
    }
    if (!m_tank_obj.load("models/tank.obj", load_file))
    {
        cerr << "Error loading tank model" << endl;
        return false;
    }
    if (!m_tile_obj.load("models/hex-tile.obj", load_file))
    {
        cerr << "Error loading hex-tile model" << endl;
        return false;
    }
    if (!m_overlay_hex_attributes.create(GL_ARRAY_BUFFER, GL_STATIC_DRAW,
                overlay_hex_attributes, sizeof(overlay_hex_attributes)))
    {
        cerr << "Error creating overlay hex attribute buffer" << endl;
        return false;
    }
    if (!m_overlay_hex_indices.create(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW,
                overlay_hex_indices, sizeof(overlay_hex_indices)))
    {
        cerr << "Error creating overlay hex indices buffer" << endl;
        return false;
    }
    m_obj_program.get_uniform_locations(obj_uniform_names, NUM_OBJ_UNIFORMS,
            obj_uniform_locations);
    m_obj_program.use();
    return true;
}

void Client::resize_window(int width, int height)
{
    m_width = width;
    m_height = height;
    sf::Mouse::setPosition(sf::Vector2i(m_width / 2, m_height / 2), *m_window);
    glViewport(0, 0, width, height);
    float aspect = (float)width / (float)height;
    m_projection.load_identity();
    m_projection.perspective(60.0f, aspect, 1.0f, 5000.0f);
}

void Client::redraw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double dir_x = cos(m_player->direction);
    double dir_y = sin(m_player->direction);
    m_modelview.load_identity();
    m_modelview.look_at(
            m_player->x - dir_x * 25, m_player->y - dir_y * 25, 30,
            m_player->x, m_player->y, 20,
            0, 0, 1);

    draw_players();
    draw_map();

    m_window->display();
}

void Client::draw_players()
{
    m_modelview.push();
    m_modelview.translate(m_player->x, m_player->y, 4);
    m_modelview.rotate(m_player->direction * 180.0 / M_PI, 0, 0, 1);
    m_tank_obj.bindBuffers();
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    int stride = m_tank_obj.getStride();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tank_obj.getVertexOffset());
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tank_obj.getNormalOffset());
    glUniform1f(OBJ_SCALE, 2.0f);
    m_projection.to_uniform(OBJ_PROJECTION);
    m_modelview.to_uniform(OBJ_MODELVIEW);
    for (map<string, WFObj::Material>::iterator it =
            m_tank_obj.getMaterials().begin();
            it != m_tank_obj.getMaterials().end();
            it++)
    {
        WFObj::Material & m = it->second;
        if (m.flags & WFObj::Material::SHININESS_BIT)
        {
            glUniform1f(OBJ_SHININESS, m.shininess);
        }
        if (m.flags & WFObj::Material::AMBIENT_BIT)
        {
            glUniform4fv(OBJ_AMBIENT, 1, &m.ambient[0]);
        }
        if (m.flags & WFObj::Material::DIFFUSE_BIT)
        {
            glUniform4fv(OBJ_DIFFUSE, 1, &m.diffuse[0]);
        }
        if (m.flags & WFObj::Material::SPECULAR_BIT)
        {
            glUniform4fv(OBJ_SPECULAR, 1, &m.specular[0]);
        }
        glDrawElements(GL_TRIANGLES, m.num_vertices,
                GL_UNSIGNED_SHORT,
                (GLvoid *) (sizeof(GLushort) * m.first_vertex));
    }
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    m_modelview.pop();
}

void Client::draw_map()
{
    const int width = m_map.get_width();
    const int height = m_map.get_height();
    m_projection.to_uniform(OBJ_PROJECTION);
    m_tile_obj.bindBuffers();
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    int stride = m_tile_obj.getStride();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tile_obj.getVertexOffset());
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tile_obj.getNormalOffset());
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (m_map.tile_present(x, y))
            {
                refptr<HexTile> tile = m_map.get_tile(x, y);
                float cx = tile->get_x();
                float cy = tile->get_y();
                m_modelview.push();
                m_modelview.translate(cx, cy, 0);
                m_modelview.to_uniform(OBJ_MODELVIEW);
                glUniform1f(OBJ_SCALE, tile->get_size());
                for (map<string, WFObj::Material>::iterator it =
                        m_tile_obj.getMaterials().begin();
                        it != m_tile_obj.getMaterials().end();
                        it++)
                {
                    WFObj::Material & m = it->second;
                    if (m.flags & WFObj::Material::SHININESS_BIT)
                    {
                        glUniform1f(OBJ_SHININESS, m.shininess);
                    }
                    if (m.flags & WFObj::Material::AMBIENT_BIT)
                    {
                        glUniform4fv(OBJ_AMBIENT, 1, &m.ambient[0]);
                    }
                    if (m.flags & WFObj::Material::DIFFUSE_BIT)
                    {
                        glUniform4fv(OBJ_DIFFUSE, 1, &m.diffuse[0]);
                    }
                    if (m.flags & WFObj::Material::SPECULAR_BIT)
                    {
                        glUniform4fv(OBJ_SPECULAR, 1, &m.specular[0]);
                    }
                    glDrawElements(GL_TRIANGLES, m.num_vertices,
                            GL_UNSIGNED_SHORT,
                            (GLvoid *) (sizeof(GLushort) * m.first_vertex));
                }
                m_modelview.pop();
            }
        }
    }
}
