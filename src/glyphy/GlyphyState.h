/*
 * Copyright 2012 Google, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef DEMO_GLSTATE_H
#define DEMO_GLSTATE_H

#include "GlyphyBuffer.h"

#include "GlyphyAtlas.h"
#include "GlyphyShader.h"
#include <memory>


class GlyphyState {
public:
    GlyphyState();
    ~GlyphyState();
    void setup();
    void fast_setup();
    std::shared_ptr<GlyphyAtlas> get_atlas() { return atlas; }
    void scale_gamma_adjust(double factor);
    void scale_contrast(double contrast);
    void toggle_debug();
    void set_color(float * color);
    void set_matrix (float mat[16]);
    void toggle_outline ();
    void scale_outline_thickness (double factor);
    void adjust_boldness (double adjustment);
    void set_depth(float depth);
    void set_uniform (GLuint program, const char *name, double *p, double value);

private:
    GLuint program;
    std::shared_ptr<GlyphyAtlas> atlas;
    /* Uniforms */
    double u_debug;
    double u_contrast;
    double u_gamma_adjust;
    double u_outline;
    double u_outline_thickness;
    double u_boldness;
    float u_color[4];
    double u_depth;
    std::shared_ptr<spdlog::logger> console;
};

#endif /* DEMO_GLSTATE_H */
