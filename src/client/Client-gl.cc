
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
    GLint uniform_locations[7];
    const char *uniforms[] = { "ambient", "diffuse", "specular", "shininess", "scale", "projection", "modelview" };
    m_obj_program.get_uniform_locations(uniforms, 7, uniform_locations);
    m_modelview.push();
    m_modelview.translate(m_player->x, m_player->y, 4);
    m_modelview.rotate(m_player->direction * 180.0 / M_PI, 0, 0, 1);
    m_obj_program.use();
    m_tank_obj.bindBuffers();
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    int stride = m_tank_obj.getStride();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tank_obj.getVertexOffset());
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            stride, (GLvoid *) m_tank_obj.getNormalOffset());
    glUniform1f(uniform_locations[4], 2.0f);
    m_projection.to_uniform(uniform_locations[5]);
    m_modelview.to_uniform(uniform_locations[6]);
    for (map<string, WFObj::Material>::iterator it =
            m_tank_obj.getMaterials().begin();
            it != m_tank_obj.getMaterials().end();
            it++)
    {
        WFObj::Material & m = it->second;
        if (m.flags & WFObj::Material::SHININESS_BIT)
        {
            glUniform1f(uniform_locations[3], m.shininess);
        }
        if (m.flags & WFObj::Material::AMBIENT_BIT)
        {
            glUniform4fv(uniform_locations[0], 1, &m.ambient[0]);
        }
        if (m.flags & WFObj::Material::DIFFUSE_BIT)
        {
            glUniform4fv(uniform_locations[1], 1, &m.diffuse[0]);
        }
        if (m.flags & WFObj::Material::SPECULAR_BIT)
        {
            glUniform4fv(uniform_locations[2], 1, &m.specular[0]);
        }
        glDrawElements(GL_TRIANGLES, m.num_vertices,
                GL_UNSIGNED_SHORT,
                (GLvoid *) (sizeof(GLushort) * m.first_vertex));
    }
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);
    m_modelview.pop();
}

void Client::draw_map()
{
    const int width = m_map.get_width();
    const int height = m_map.get_height();
    const float tile_size = 50;
    GLint uniform_locations[7];
    const char *uniforms[] = { "ambient", "diffuse", "specular", "shininess", "scale", "projection", "modelview" };
    m_obj_program.get_uniform_locations(uniforms, 7, uniform_locations);
    m_obj_program.use();
    glUniform1f(uniform_locations[4], tile_size);
    m_projection.to_uniform(uniform_locations[5]);
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
                m_modelview.push();
                float cx = m_map.get_tile(x, y)->get_x();
                float cy = m_map.get_tile(x, y)->get_y();
                m_modelview.translate(cx, cy, 0);
                m_modelview.to_uniform(uniform_locations[6]);
                for (map<string, WFObj::Material>::iterator it =
                        m_tile_obj.getMaterials().begin();
                        it != m_tile_obj.getMaterials().end();
                        it++)
                {
                    WFObj::Material & m = it->second;
                    if (m.flags & WFObj::Material::SHININESS_BIT)
                    {
                        glUniform1f(uniform_locations[3], m.shininess);
                    }
                    if (m.flags & WFObj::Material::AMBIENT_BIT)
                    {
                        glUniform4fv(uniform_locations[0], 1, &m.ambient[0]);
                    }
                    if (m.flags & WFObj::Material::DIFFUSE_BIT)
                    {
                        glUniform4fv(uniform_locations[1], 1, &m.diffuse[0]);
                    }
                    if (m.flags & WFObj::Material::SPECULAR_BIT)
                    {
                        glUniform4fv(uniform_locations[2], 1, &m.specular[0]);
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