#!/usr/bin/env python3
"""Gera userland/libsystem/gl_enum.h (constantes) e gl_proto.h (protótipos) a partir do gl.xml do
registro da Khronos (Apache-2.0). Cobre GL 1.0-4.1 (core + compatibility) e as extensões que o
OpenGL do macOS anuncia. Uso: tools/gl_gen.py caminho/gl.xml"""
import sys, re, xml.etree.ElementTree as ET
root = ET.parse(sys.argv[1]).getroot()
VERS = ['1.0','1.1','1.2','1.3','1.4','1.5','2.0','2.1','3.0','3.1','3.2','3.3','4.0','4.1']
EXTS = ['GL_ARB_vertex_buffer_object','GL_ARB_shader_objects','GL_ARB_vertex_shader','GL_ARB_fragment_shader',
        'GL_ARB_vertex_program','GL_EXT_framebuffer_object','GL_EXT_framebuffer_blit','GL_EXT_framebuffer_multisample',
        'GL_ARB_texture_rectangle','GL_EXT_texture_compression_s3tc','GL_EXT_texture_filter_anisotropic',
        'GL_APPLE_vertex_array_object','GL_APPLE_fence','GL_APPLE_flush_buffer_range','GL_APPLE_client_storage',
        'GL_APPLE_ycbcr_422','GL_APPLE_texture_range','GL_APPLE_row_bytes','GL_APPLE_element_array',
        'GL_APPLE_vertex_array_range','GL_APPLE_object_purgeable','GL_APPLE_rgb_422','GL_EXT_texture_sRGB',
        'GL_EXT_blend_func_separate','GL_EXT_blend_equation_separate','GL_EXT_blend_minmax','GL_EXT_blend_color',
        'GL_ARB_multitexture','GL_ARB_texture_compression','GL_ARB_occlusion_query','GL_ARB_draw_buffers',
        'GL_EXT_gpu_program_parameters','GL_ARB_instanced_arrays','GL_EXT_draw_buffers2','GL_EXT_texture_array',
        'GL_EXT_transform_feedback','GL_EXT_gpu_shader4','GL_ARB_color_buffer_float','GL_EXT_packed_depth_stencil',
        'GL_EXT_texture_compression_rgtc','GL_ARB_texture_compression_bptc','GL_EXT_debug_label','GL_EXT_debug_marker',
        'GL_ARB_point_sprite','GL_ARB_window_pos','GL_EXT_stencil_two_side','GL_ATI_separate_stencil',
        'GL_ARB_vertex_array_bgra','GL_EXT_bindable_uniform','GL_EXT_texture_integer','GL_ARB_shading_language_100',
        'GL_EXT_framebuffer_sRGB','GL_NV_conditional_render','GL_ARB_sync','GL_ARB_timer_query',
        'GL_ARB_get_program_binary','GL_ARB_separate_shader_objects','GL_EXT_provoking_vertex','GL_ARB_provoking_vertex',
        'GL_ARB_texture_storage','GL_EXT_texture_mirror_clamp','GL_ATI_texture_mirror_once','GL_EXT_secondary_color',
        'GL_EXT_fog_coord','GL_ARB_point_parameters','GL_EXT_multi_draw_arrays','GL_ARB_transpose_matrix',
        'GL_EXT_geometry_shader4','GL_ARB_geometry_shader4','GL_EXT_draw_instanced','GL_ARB_draw_instanced',
        'GL_ARB_vertex_type_2_10_10_10_rev','GL_ARB_ES2_compatibility','GL_ARB_texture_swizzle']
ENUM_ONLY = ['GL_KHR_debug', 'GL_ARB_imaging', 'GL_ARB_copy_buffer', 'GL_ARB_debug_output', 'GL_ARB_texture_float', 'GL_EXT_abgr', 'GL_ARB_texture_mirror_clamp_to_edge', 'GL_EXT_texture_sRGB_decode', 'GL_ARB_texture_view', 'GL_ARB_buffer_storage', 'GL_ARB_texture_cube_map_array', 'GL_ARB_depth_buffer_float', 'GL_ARB_texture_rg', 'GL_ARB_framebuffer_object', 'GL_ARB_half_float_vertex', 'GL_ARB_vertex_array_object', 'GL_ARB_internalformat_query']
want_enum, want_cmd = set(), {}
def take(req, tag):
    for e in req.findall('enum'): want_enum.add(e.get('name'))
    for c in req.findall('command'): want_cmd.setdefault(c.get('name'), tag)
for f in root.findall('feature'):
    if f.get('api') == 'gl' and f.get('number') in VERS:
        for r in f.findall('require'): take(r, f.get('number'))
for x in root.find('extensions'):
    if x.get('name') in EXTS:
        for r in x.findall('require'):
            if r.get('api') in (None, 'gl'): take(r, x.get('name'))
    if x.get('name') in ENUM_ONLY:
        for r in x.findall('require'):
            for e in r.findall('enum'): want_enum.add(e.get('name'))
vals = {}
for es in root.findall('enums'):
    for e in es.findall('enum'):
        if e.get('api') not in (None, 'gl'): continue
        vals[e.get('name')] = e.get('value')
want_enum |= {'GL_FALSE','GL_TRUE','GL_NONE','GL_ZERO','GL_ONE'}
out = ['/* Gerado por tools/gl_gen.py a partir do gl.xml da Khronos (Apache-2.0). Não editar. */', '#pragma once']
for n in sorted(want_enum):
    if n in vals: out.append('#define %s %s' % (n, vals[n]))
open(sys.argv[2] + '/gl_enum.h', 'w').write('\n'.join(out) + '\n')
def txt(el):
    s = (el.text or '')
    for ch in el:
        s += txt(ch) + (ch.tail or '')
    return s
protos = ['/* Gerado por tools/gl_gen.py a partir do gl.xml da Khronos (Apache-2.0). Não editar.',
          ' * Protótipos oficiais: o compilador confere as assinaturas do OpenGL do mOS. */', '#pragma once',
          'typedef struct __GLsync *GLsync;', 'typedef void (*GLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const char *, const void *);',
          'typedef void *GLhandleARB;', 'typedef char GLchar;', 'typedef char GLcharARB;',
          'typedef signed char GLbyte;', 'typedef unsigned char GLubyte;', 'typedef short GLshort;', 'typedef unsigned short GLushort;',
          'typedef long long GLint64;', 'typedef unsigned long long GLuint64;', 'typedef void GLvoid;', 'typedef float GLclampf;',
          'typedef double GLclampd;', 'typedef unsigned short GLhalf;', 'typedef unsigned short GLhalfARB;',
          'typedef long GLintptrARB;', 'typedef long GLsizeiptrARB;', 'typedef long long GLint64EXT;', 'typedef unsigned long long GLuint64EXT;']
names = []
for c in root.find('commands').findall('command'):
    p = c.find('proto'); n = p.find('name').text
    if n not in want_cmd: continue
    ret = txt(p)[:-len(n)].strip()
    ps = [re.sub(r'\s+', ' ', txt(q)).strip() for q in c.findall('param')]
    protos.append('%s %s(%s);' % (ret, n, ', '.join(ps) or 'void'))
    names.append((n, want_cmd[n]))
open(sys.argv[2] + '/gl_proto.h', 'w').write('\n'.join(protos) + '\n')
open(sys.argv[3], 'w').write(''.join('%s %s\n' % x for x in sorted(names)))
print(len(want_enum), 'enums', len(names), 'comandos')
