/* RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt). */
/* d3gltest: does this GL do what dhewm3's renderer needs?
 *
 * dhewm3 draws only through its ARB2 back end (renderer/draw_arb2.cpp): ARB vertex and
 * fragment programs, vertex attributes 8-11, VBOs with indices from the program's memory,
 * cube maps, two-sided stencil shadow volumes (z-pass and z-fail), depth bounds and S3TC.
 * Each check draws something small, reads the pixels back and compares them with what it
 * should be. Then it times the things that cost a round trip on a command stream
 * (glGetError, glReadPixels, glFinish) and a short interaction-shader benchmark.
 *
 * Output: "PASS name", "FAIL name: why" or "SKIP name: why" lines on stdout, then
 * "RESULT n passed, n failed, n skipped". Exit status 0 only if nothing failed.
 *
 *   d3gltest [-bench frames] [-w width -h height] [-nocanary]
 *
 * Builds for RISC OS (riscos/sdk.sh tests/d3gltest/build.sh) and anywhere with SDL2.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define GL_GLEXT_PROTOTYPES 0
#include <SDL_opengl.h>

/* --- the functions dhewm3 loads (renderer/qgl_proc.h) ------------------- */
typedef void (APIENTRY *P_GenProgramsARB)(GLsizei, GLuint *);
typedef void (APIENTRY *P_BindProgramARB)(GLenum, GLuint);
typedef void (APIENTRY *P_ProgramStringARB)(GLenum, GLenum, GLsizei, const void *);
typedef void (APIENTRY *P_ProgramEnvParameter4fvARB)(GLenum, GLuint, const GLfloat *);
typedef void (APIENTRY *P_ProgramLocalParameter4fvARB)(GLenum, GLuint, const GLfloat *);
typedef void (APIENTRY *P_VertexAttribPointerARB)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void (APIENTRY *P_EnableVertexAttribArrayARB)(GLuint);
typedef void (APIENTRY *P_DisableVertexAttribArrayARB)(GLuint);
typedef void (APIENTRY *P_ActiveTextureARB)(GLenum);
typedef void (APIENTRY *P_ClientActiveTextureARB)(GLenum);
typedef void (APIENTRY *P_GenBuffersARB)(GLsizei, GLuint *);
typedef void (APIENTRY *P_BindBufferARB)(GLenum, GLuint);
typedef void (APIENTRY *P_BufferDataARB)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (APIENTRY *P_BufferSubDataARB)(GLenum, GLintptr, GLsizeiptr, const void *);
typedef void (APIENTRY *P_ActiveStencilFaceEXT)(GLenum);
typedef void (APIENTRY *P_StencilOpSeparateATI)(GLenum, GLenum, GLenum, GLenum);
typedef void (APIENTRY *P_StencilFuncSeparateATI)(GLenum, GLenum, GLint, GLuint);
typedef void (APIENTRY *P_DepthBoundsEXT)(GLclampd, GLclampd);
typedef void (APIENTRY *P_CompressedTexImage2DARB)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);

static P_GenProgramsARB pGenProgramsARB;
static P_BindProgramARB pBindProgramARB;
static P_ProgramStringARB pProgramStringARB;
static P_ProgramEnvParameter4fvARB pProgramEnvParameter4fvARB;
static P_ProgramLocalParameter4fvARB pProgramLocalParameter4fvARB;
static P_VertexAttribPointerARB pVertexAttribPointerARB;
static P_EnableVertexAttribArrayARB pEnableVertexAttribArrayARB;
static P_DisableVertexAttribArrayARB pDisableVertexAttribArrayARB;
static P_ActiveTextureARB pActiveTextureARB;
static P_ClientActiveTextureARB pClientActiveTextureARB;
static P_GenBuffersARB pGenBuffersARB;
static P_BindBufferARB pBindBufferARB;
static P_BufferDataARB pBufferDataARB;
static P_BufferSubDataARB pBufferSubDataARB;
static P_ActiveStencilFaceEXT pActiveStencilFaceEXT;
static P_StencilOpSeparateATI pStencilOpSeparateATI;
static P_StencilFuncSeparateATI pStencilFuncSeparateATI;
static P_DepthBoundsEXT pDepthBoundsEXT;
static P_CompressedTexImage2DARB pCompressedTexImage2DARB;

#ifndef GL_VERTEX_PROGRAM_ARB
#define GL_VERTEX_PROGRAM_ARB 0x8620
#define GL_FRAGMENT_PROGRAM_ARB 0x8804
#define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#define GL_PROGRAM_ERROR_POSITION_ARB 0x864B
#define GL_PROGRAM_ERROR_STRING_ARB 0x8874
#endif
#ifndef GL_ARRAY_BUFFER_ARB
#define GL_ARRAY_BUFFER_ARB 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB 0x8893
#define GL_STATIC_DRAW_ARB 0x88E4
#define GL_STREAM_DRAW_ARB 0x88E0
#endif
#ifndef GL_STENCIL_TEST_TWO_SIDE_EXT
#define GL_STENCIL_TEST_TWO_SIDE_EXT 0x8910
#endif
#ifndef GL_INCR_WRAP_EXT
#define GL_INCR_WRAP_EXT 0x8507
#define GL_DECR_WRAP_EXT 0x8508
#endif
#ifndef GL_DEPTH_BOUNDS_TEST_EXT
#define GL_DEPTH_BOUNDS_TEST_EXT 0x8890
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_TEXTURE_CUBE_MAP_ARB
#define GL_TEXTURE_CUBE_MAP_ARB 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
#endif
#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#endif

static int W = 256, H = 256;
static int npass, nfail, nskip;
static SDL_Window *win;

static void out(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fflush(stdout);
}
static void pass(const char *n) { npass++; out("PASS %s\n", n); }
static void skip(const char *n, const char *why) { nskip++; out("SKIP %s: %s\n", n, why); }
static void fail(const char *n, const char *fmt, ...)
{
	char b[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof b, fmt, ap);
	va_end(ap);
	nfail++;
	out("FAIL %s: %s\n", n, b);
}

static int has_ext(const char *name)
{
	const char *e = (const char *)glGetString(GL_EXTENSIONS);
	size_t l = strlen(name);
	while (e && (e = strstr(e, name)) != NULL) {
		if (e[l] == ' ' || e[l] == 0)
			return 1;
		e += l;
	}
	return 0;
}

static void *proc(const char *a, const char *b)
{
	void *p = SDL_GL_GetProcAddress(a);
	if (!p && b)
		p = SDL_GL_GetProcAddress(b);
	return p;
}

/* Reads one pixel (window coordinates, origin bottom left). */
static void px(int x, int y, unsigned char c[4])
{
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
}

/* Is the pixel at x,y within tol of r,g,b? Reports a failure and returns 0 if not. */
static int expect(const char *name, int x, int y, int r, int g, int b, int tol)
{
	unsigned char c[4];
	px(x, y, c);
	if (abs(c[0] - r) > tol || abs(c[1] - g) > tol || abs(c[2] - b) > tol) {
		fail(name, "pixel %d,%d is %d,%d,%d, expected %d,%d,%d (+-%d)", x, y, c[0], c[1], c[2], r, g, b, tol);
		return 0;
	}
	return 1;
}

static int gl_errors(const char *name)
{
	GLenum e;
	int n = 0;
	while ((e = glGetError()) != GL_NO_ERROR && n < 10) {
		if (name)
			fail(name, "glGetError 0x%04x", e);
		n++;
	}
	return n;
}

/* Screen-space projection: x 0..W, y 0..H, and z such that depth = d at z = 1 - 2d. */
static void ortho(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, W, 0, H, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
static float zd(float d) { return 1.0f - 2.0f * d; }

static void quad(float x0, float y0, float x1, float y1, float z)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z);
	glTexCoord2f(1, 1); glVertex3f(x1, y1, z);
	glTexCoord2f(0, 1); glVertex3f(x0, y1, z);
	glEnd();
}
/* The same quad wound clockwise (a back face). */
static void quad_cw(float x0, float y0, float x1, float y1, float z)
{
	glBegin(GL_QUADS);
	glVertex3f(x0, y0, z); glVertex3f(x0, y1, z); glVertex3f(x1, y1, z); glVertex3f(x1, y0, z);
	glEnd();
}

static void reset_state(void)
{
	glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
	glColorMask(1, 1, 1, 1); glDepthMask(1); glStencilMask(0xff);
	glDepthFunc(GL_LESS);
	glClearColor(0, 0, 0, 1); glClearDepth(1); glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glColor4f(1, 1, 1, 1);
	ortho();
}

/* --- 1. what's there ---------------------------------------------------- */
static const char *const needed[] = {	/* dhewm3 can't start without these */
	"GL_ARB_multitexture", "GL_ARB_vertex_program", "GL_ARB_fragment_program", NULL };
static const char *const wanted[] = {	/* it uses these if they're there */
	"GL_ARB_texture_env_combine", "GL_ARB_texture_cube_map", "GL_ARB_texture_env_dot3",
	"GL_ARB_texture_env_add", "GL_ARB_texture_non_power_of_two", "GL_ARB_texture_compression",
	"GL_EXT_texture_compression_s3tc", "GL_EXT_texture_filter_anisotropic", "GL_EXT_texture_lod",
	"GL_EXT_stencil_wrap", "GL_EXT_stencil_two_side", "GL_ATI_separate_stencil",
	"GL_ARB_vertex_buffer_object", "GL_EXT_depth_bounds_test", "GL_ARB_texture_compression_bptc",
	NULL };

static void t_strings(void)
{
	int i, ok = 1;
	GLint v = 0;
	out("INFO GL_VENDOR %s\n", glGetString(GL_VENDOR));
	out("INFO GL_RENDERER %s\n", glGetString(GL_RENDERER));
	out("INFO GL_VERSION %s\n", glGetString(GL_VERSION));
	glGetIntegerv(GL_STENCIL_BITS, &v); out("INFO stencil bits %d\n", v);
	glGetIntegerv(GL_DEPTH_BITS, &v); out("INFO depth bits %d\n", v);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v); out("INFO max texture size %d\n", v);
	glGetIntegerv(0x84E2 /* GL_MAX_TEXTURE_UNITS_ARB */, &v); out("INFO texture units %d\n", v);
	for (i = 0; needed[i]; i++)
		if (!has_ext(needed[i])) {
			fail("extensions", "missing %s (dhewm3 needs it)", needed[i]);
			ok = 0;
		}
	for (i = 0; wanted[i]; i++)
		out("INFO %s %s\n", has_ext(wanted[i]) ? "has" : "no ", wanted[i]);
	if (ok)
		pass("extensions");
}

static void load_procs(void)
{
	pGenProgramsARB = proc("glGenProgramsARB", NULL);
	pBindProgramARB = proc("glBindProgramARB", NULL);
	pProgramStringARB = proc("glProgramStringARB", NULL);
	pProgramEnvParameter4fvARB = proc("glProgramEnvParameter4fvARB", NULL);
	pProgramLocalParameter4fvARB = proc("glProgramLocalParameter4fvARB", NULL);
	pVertexAttribPointerARB = proc("glVertexAttribPointerARB", "glVertexAttribPointer");
	pEnableVertexAttribArrayARB = proc("glEnableVertexAttribArrayARB", "glEnableVertexAttribArray");
	pDisableVertexAttribArrayARB = proc("glDisableVertexAttribArrayARB", "glDisableVertexAttribArray");
	pActiveTextureARB = proc("glActiveTextureARB", "glActiveTexture");
	pClientActiveTextureARB = proc("glClientActiveTextureARB", "glClientActiveTexture");
	pGenBuffersARB = proc("glGenBuffersARB", "glGenBuffers");
	pBindBufferARB = proc("glBindBufferARB", "glBindBuffer");
	pBufferDataARB = proc("glBufferDataARB", "glBufferData");
	pBufferSubDataARB = proc("glBufferSubDataARB", "glBufferSubData");
	pActiveStencilFaceEXT = proc("glActiveStencilFaceEXT", NULL);
	pStencilOpSeparateATI = proc("glStencilOpSeparateATI", "glStencilOpSeparate");
	pStencilFuncSeparateATI = proc("glStencilFuncSeparateATI", NULL);
	pDepthBoundsEXT = proc("glDepthBoundsEXT", NULL);
	pCompressedTexImage2DARB = proc("glCompressedTexImage2DARB", "glCompressedTexImage2D");
}

/* --- 2. ARB programs: Doom 3's interaction, cut down -------------------- */
/* Like interaction.vfp: positions through state.matrix.mvp, the texture coordinate in
 * attribute 8 and the light vector in attribute 9 (dhewm3 binds attributes 8-11), a
 * normal map and a normalisation cube map dotted together, times a colour in env[0]. */
static const char vp_src[] =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"
	"MOV result.texcoord[0], vertex.attrib[8];\n"
	"MOV result.texcoord[1], vertex.attrib[9];\n"
	"END\n";
static const char vp2_src[] =		/* the same through the matrix by hand */
	"!!ARBvp1.0\n"
	"PARAM mvp[4] = { state.matrix.mvp };\n"
	"DP4 result.position.x, mvp[0], vertex.position;\n"
	"DP4 result.position.y, mvp[1], vertex.position;\n"
	"DP4 result.position.z, mvp[2], vertex.position;\n"
	"DP4 result.position.w, mvp[3], vertex.position;\n"
	"MOV result.texcoord[0], vertex.attrib[8];\n"
	"MOV result.texcoord[1], vertex.attrib[9];\n"
	"END\n";
static const char fp_src[] =
	"!!ARBfp1.0\n"
	"OPTION ARB_precision_hint_fastest;\n"
	"PARAM two = { 2, 2, 2, 2 };\n"
	"PARAM mone = { -1, -1, -1, -1 };\n"
	"TEMP n, l, c;\n"
	"TEX n, fragment.texcoord[0], texture[0], 2D;\n"
	"MAD n, n, two, mone;\n"
	"TEX l, fragment.texcoord[1], texture[1], CUBE;\n"
	"MAD l, l, two, mone;\n"
	"DP3_SAT c, n, l;\n"
	"MUL result.color, c, program.env[0];\n"
	"END\n";

static GLuint prog(GLenum target, const char *src, const char *name)
{
	GLuint p = 0;
	GLint pos = -1;
	pGenProgramsARB(1, &p);
	pBindProgramARB(target, p);
	pProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(src), src);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &pos);
	if (pos != -1) {
		fail(name, "program error at %d: %s", pos, (const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB));
		return 0;
	}
	return p;
}

static GLuint tex_normal, tex_cube, vp, vp2, fp;

static void make_textures(void)
{
	unsigned char nm[16 * 16 * 4];
	unsigned char face[4 * 4 * 4];
	/* each cube face is the vector it faces, encoded as (v + 1) / 2 */
	static const int dir[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
	int i, f;
	for (i = 0; i < 16 * 16; i++) {	/* flat normal map: (0, 0, 1) */
		nm[i * 4 + 0] = 128; nm[i * 4 + 1] = 128; nm[i * 4 + 2] = 255; nm[i * 4 + 3] = 255;
	}
	glGenTextures(1, &tex_normal);
	glBindTexture(GL_TEXTURE_2D, tex_normal);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nm);

	glGenTextures(1, &tex_cube);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, tex_cube);
	for (f = 0; f < 6; f++) {
		for (i = 0; i < 16; i++) {
			face[i * 4 + 0] = (unsigned char)((dir[f][0] + 1) * 255 / 2);
			face[i * 4 + 1] = (unsigned char)((dir[f][1] + 1) * 255 / 2);
			face[i * 4 + 2] = (unsigned char)((dir[f][2] + 1) * 255 / 2);
			face[i * 4 + 3] = 255;
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + f, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, face);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

/* A quad from x0 to x1 with the light vector lx,ly,lz, drawn as dhewm3 draws:
 * vertex arrays, attributes 8 and 9, glDrawElements with indices in the program's
 * memory, optionally with the vertices in a VBO. */
struct ivert { float xyz[3]; float st[2]; float light[3]; };
static const GLushort quad_idx[6] = { 0, 1, 2, 0, 2, 3 };

static void fill_quad(struct ivert *v, float x0, float y0, float x1, float y1, float lx, float ly, float lz)
{
	int i;
	float xs[4] = { x0, x1, x1, x0 }, ys[4] = { y0, y0, y1, y1 };
	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = xs[i]; v[i].xyz[1] = ys[i]; v[i].xyz[2] = 0;
		v[i].st[0] = (float)(i == 1 || i == 2); v[i].st[1] = (float)(i >= 2);
		v[i].light[0] = lx; v[i].light[1] = ly; v[i].light[2] = lz;
	}
}

static void bind_interaction(GLuint v)
{
	static const float colour[4] = { 0.5f, 0.25f, 0.75f, 1.0f };
	glEnable(GL_VERTEX_PROGRAM_ARB);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	pBindProgramARB(GL_VERTEX_PROGRAM_ARB, v);
	pBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
	pProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 0, colour);
	pActiveTextureARB(GL_TEXTURE0_ARB + 1);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, tex_cube);
	pActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, tex_normal);
}
static void unbind_interaction(void)
{
	glDisable(GL_VERTEX_PROGRAM_ARB);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

static void draw_ivert(const struct ivert *base /* NULL: offsets into the bound VBO */)
{
	const char *b = (const char *)base;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(struct ivert), b + 0);
	pEnableVertexAttribArrayARB(8);
	pEnableVertexAttribArrayARB(9);
	pVertexAttribPointerARB(8, 2, GL_FLOAT, GL_FALSE, sizeof(struct ivert), b + 12);
	pVertexAttribPointerARB(9, 3, GL_FLOAT, GL_FALSE, sizeof(struct ivert), b + 20);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quad_idx);
	pDisableVertexAttribArrayARB(8);
	pDisableVertexAttribArrayARB(9);
	glDisableClientState(GL_VERTEX_ARRAY);
}

static void t_programs(void)
{
	struct ivert v[8];
	int ok = 1;
	if (!pGenProgramsARB || !pProgramStringARB || !pVertexAttribPointerARB || !pActiveTextureARB) {
		fail("arb-programs", "functions missing");
		return;
	}
	reset_state();
	vp = prog(GL_VERTEX_PROGRAM_ARB, vp_src, "arb-vp-compile");
	vp2 = prog(GL_VERTEX_PROGRAM_ARB, vp2_src, "arb-vp-mvp-compile");
	fp = prog(GL_FRAGMENT_PROGRAM_ARB, fp_src, "arb-fp-compile");
	if (!vp || !vp2 || !fp)
		return;
	make_textures();
	if (gl_errors("arb-setup"))
		return;
	pass("arb-compile");

	/* left half lit from +Z (facing the normal: the full colour), right half from +X
	 * (side on: black) */
	bind_interaction(vp);
	fill_quad(v, 0, 0, W / 2, H, 0, 0, 1);
	fill_quad(v + 4, W / 2, 0, W, H, 1, 0, 0);
	draw_ivert(v);
	draw_ivert(v + 4);
	unbind_interaction();
	ok &= expect("arb-interaction", W / 4, H / 2, 128, 64, 191, 6);
	ok &= expect("arb-interaction", 3 * W / 4, H / 2, 0, 0, 0, 6);
	if (ok && !gl_errors("arb-interaction"))
		pass("arb-interaction");

	/* the same, transformed by hand through state.matrix.mvp, with a translation in the
	 * modelview matrix (it has to reach the program) */
	reset_state();
	glTranslatef(W / 2, 0, 0);
	bind_interaction(vp2);
	fill_quad(v, 0, 0, W / 2, H, 0, 0, 1);
	draw_ivert(v);
	unbind_interaction();
	ok = expect("arb-state-matrix-mvp", 3 * W / 4, H / 2, 128, 64, 191, 6);
	ok &= expect("arb-state-matrix-mvp", W / 4, H / 2, 0, 0, 0, 6);
	if (ok && !gl_errors("arb-state-matrix-mvp"))
		pass("arb-state-matrix-mvp");
}

/* --- 3. VBOs, as dhewm3's vertex cache uses them ------------------------- */
static void t_vbo(void)
{
	struct ivert v[4];
	GLuint b[2];
	int ok;
	if (!pGenBuffersARB || !fp) {
		skip("vbo", "no VBO functions or programs");
		return;
	}
	reset_state();
	pGenBuffersARB(2, b);
	/* vertices in a VBO, indices in the program's memory (r_useIndexBuffers 0) */
	fill_quad(v, 0, 0, W, H, 0, 0, 1);
	pBindBufferARB(GL_ARRAY_BUFFER_ARB, b[0]);
	pBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof v, NULL, GL_STREAM_DRAW_ARB);
	pBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof v, v);	/* as the frame-temp cache */
	bind_interaction(vp);
	draw_ivert(NULL);
	ok = expect("vbo-client-indices", W / 2, H / 2, 128, 64, 191, 6);
	if (ok && !gl_errors("vbo-client-indices"))
		pass("vbo-client-indices");

	/* and indices in a VBO too (r_useIndexBuffers 1) */
	reset_state();
	bind_interaction(vp);
	pBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, b[1]);
	pBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof quad_idx, quad_idx, GL_STATIC_DRAW_ARB);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(struct ivert), (void *)0);
	pEnableVertexAttribArrayARB(8);
	pEnableVertexAttribArrayARB(9);
	pVertexAttribPointerARB(8, 2, GL_FLOAT, GL_FALSE, sizeof(struct ivert), (void *)12);
	pVertexAttribPointerARB(9, 3, GL_FLOAT, GL_FALSE, sizeof(struct ivert), (void *)20);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);
	pDisableVertexAttribArrayARB(8);
	pDisableVertexAttribArrayARB(9);
	glDisableClientState(GL_VERTEX_ARRAY);
	unbind_interaction();
	pBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	pBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	ok = expect("vbo-buffer-indices", W / 2, H / 2, 128, 64, 191, 6);
	if (ok && !gl_errors("vbo-buffer-indices"))
		pass("vbo-buffer-indices");
}

/* --- 4. stencil shadow volumes ----------------------------------------- */
/* Three columns: a front face over the left two thirds, a back face over the right two
 * thirds. Z-pass: front faces increment, back faces decrement, so the stencil is 1, 0,
 * -1 (255) across. Then everything where the stencil isn't 0 is painted red, the rest
 * green: red, green, red. Z-fail (Carmack's reverse, which dhewm3 uses when the eye is
 * inside a volume) does the same on depth failure, with the faces behind a floor. */
enum { TWO_SIDE, ATI_SEP };

static void shadow_faces(int how, int zfail)
{
	float z = zfail ? zd(0.7f) : zd(0.3f);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 0, 0xff);
	glColorMask(0, 0, 0, 0);
	glDepthMask(0);
	if (how == TWO_SIDE) {
		glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
		pActiveStencilFaceEXT(GL_BACK);
		if (zfail) glStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
		else glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
		pActiveStencilFaceEXT(GL_FRONT);
		if (zfail) glStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
		else glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
	} else {
		if (zfail) {
			pStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
			pStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
		} else {
			pStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
			pStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
		}
	}
	quad(0, 0, 2 * W / 3, H, z);	/* counter-clockwise: front */
	quad_cw(W / 3, 0, W, H, z);	/* clockwise: back */
	if (how == TWO_SIDE)
		glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	glColorMask(1, 1, 1, 1);
	glDepthMask(1);
}

static void t_stencil_one(const char *name, int how, int zfail)
{
	int ok;
	reset_state();
	glEnable(GL_DEPTH_TEST);
	if (zfail) {		/* a floor at depth 0.5, in front of the faces */
		glDepthFunc(GL_LESS);
		glColorMask(0, 0, 0, 0);
		quad(0, 0, W, H, zd(0.5f));
		glColorMask(1, 1, 1, 1);
	}
	shadow_faces(how, zfail);
	glDisable(GL_DEPTH_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_NOTEQUAL, 0, 0xff);
	glColor3f(1, 0, 0); quad(0, 0, W, H, 0);
	glStencilFunc(GL_EQUAL, 0, 0xff);
	glColor3f(0, 1, 0); quad(0, 0, W, H, 0);
	glDisable(GL_STENCIL_TEST);
	ok = expect(name, W / 6, H / 2, 255, 0, 0, 2);
	ok &= expect(name, W / 2, H / 2, 0, 255, 0, 2);
	ok &= expect(name, 5 * W / 6, H / 2, 255, 0, 0, 2);
	if (ok && !gl_errors(name))
		pass(name);
}

static void t_stencil(void)
{
	GLint bits = 0;
	glGetIntegerv(GL_STENCIL_BITS, &bits);
	if (bits < 8) {
		fail("stencil", "only %d stencil bits (dhewm3 asks for 8)", bits);
		return;
	}
	if (has_ext("GL_EXT_stencil_two_side") && pActiveStencilFaceEXT) {
		t_stencil_one("stencil-two-side-zpass", TWO_SIDE, 0);
		t_stencil_one("stencil-two-side-zfail", TWO_SIDE, 1);
	} else
		skip("stencil-two-side", "no GL_EXT_stencil_two_side");
	if (has_ext("GL_ATI_separate_stencil") && pStencilOpSeparateATI) {
		t_stencil_one("stencil-ati-separate-zpass", ATI_SEP, 0);
		t_stencil_one("stencil-ati-separate-zfail", ATI_SEP, 1);
	} else
		skip("stencil-ati-separate", "no GL_ATI_separate_stencil");
}

/* --- 5. depth bounds (dhewm3 uses it to cut lights' stencil and interactions) --- */
static void t_depth_bounds(void)
{
	int ok;
	if (!has_ext("GL_EXT_depth_bounds_test") || !pDepthBoundsEXT) {
		skip("depth-bounds", "no GL_EXT_depth_bounds_test (dhewm3 copes without)");
		return;
	}
	reset_state();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);
	glColorMask(0, 0, 0, 0);
	quad(0, 0, W / 2, H, zd(0.5f));
	quad(W / 2, 0, W, H, zd(0.9f));
	glColorMask(1, 1, 1, 1);
	glDepthMask(0);
	glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
	pDepthBoundsEXT(0.4, 0.6);
	glColor3f(0, 0, 1);
	quad(0, 0, W, H, zd(0.1f));
	glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
	ok = expect("depth-bounds", W / 4, H / 2, 0, 0, 255, 2);
	ok &= expect("depth-bounds", 3 * W / 4, H / 2, 0, 0, 0, 2);
	if (ok && !gl_errors("depth-bounds"))
		pass("depth-bounds");
}

/* --- 6. S3TC (Doom 3's precompressed .dds images) ------------------------ */
static void t_s3tc(void)
{
	/* one DXT1 block: colour 0 red, colour 1 blue (4-colour mode, c0 > c1); rows use
	 * index 0, 1, 2, 3 */
	static const unsigned char block[8] = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x55, 0xAA, 0xFF };
	static const int want[4][3] = { {255,0,0}, {0,0,255}, {170,0,85}, {85,0,170} };
	GLuint t;
	int r, ok = 1;
	if (!has_ext("GL_EXT_texture_compression_s3tc") || !pCompressedTexImage2DARB) {
		skip("s3tc", "no GL_EXT_texture_compression_s3tc (dhewm3 then uses the uncompressed images)");
		return;
	}
	reset_state();
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	pCompressedTexImage2DARB(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, block);
	if (gl_errors("s3tc"))
		return;
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	quad(0, 0, W, H, 0);
	glDisable(GL_TEXTURE_2D);
	for (r = 0; r < 4; r++)	/* texture row r is at the bottom + r quarters */
		ok &= expect("s3tc", W / 2, (2 * r + 1) * H / 8, want[r][0], want[r][1], want[r][2], 10);
	if (ok && !gl_errors("s3tc"))
		pass("s3tc");
}

/* A plain immediate-mode quad: a canary run between the stages, so a stage that leaves
 * state behind shows up where it happens. */
static int no_canary;
static void t_canary(const char *after)
{
	char n[64];
	if (no_canary)
		return;
	reset_state();
	glColor3f(1, 0.5f, 0);
	quad(0, 0, W, H, 0);
	snprintf(n, sizeof n, "immediate-after-%s", after);
	if (expect(n, W / 2, H / 2, 255, 128, 0, 3) && !gl_errors(n))
		pass(n);
}

/* --- 7. the fixed-function bits dhewm3 still uses ----------------------- */
static void t_fixed(void)
{
	int ok;
	reset_state();
	/* blend the way the light's additive passes do: GL_ONE, GL_ONE */
	glColor3f(0.25f, 0.5f, 0.0f); quad(0, 0, W, H, 0);
	glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
	glColor3f(0.25f, 0.0f, 0.5f); quad(0, 0, W, H, 0);
	glDisable(GL_BLEND);
	ok = expect("blend-additive", W / 2, H / 2, 128, 128, 128, 3);
	/* the depth-only pre-pass then GL_EQUAL, as dhewm3 draws every interaction */
	reset_state();
	glEnable(GL_DEPTH_TEST);
	glColorMask(0, 0, 0, 0); quad(0, 0, W, H, zd(0.5f)); glColorMask(1, 1, 1, 1);
	glDepthFunc(GL_EQUAL); glDepthMask(0);
	glColor3f(1, 1, 0); quad(0, 0, W / 2, H, zd(0.5f)); quad(W / 2, 0, W, H, zd(0.6f));
	ok &= expect("depth-equal", W / 4, H / 2, 255, 255, 0, 2);
	ok &= expect("depth-equal", 3 * W / 4, H / 2, 0, 0, 0, 2);
	if (ok && !gl_errors("fixed-function"))
		pass("fixed-function");
}

/* --- 8. timing ----------------------------------------------------------- */
static double now_ms(void) { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }

static void t_timing(int frames)
{
	int i, f;
	double t;
	unsigned char c[4];
	struct ivert *v;
	enum { QUADS = 64 };

	t = now_ms();
	for (i = 0; i < 1000; i++)
		glGetError();
	out("TIME glGetError %.1f us each (dhewm3's GL_CheckErrors calls it every check)\n", (now_ms() - t));
	t = now_ms();
	for (i = 0; i < 200; i++)
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
	out("TIME glReadPixels 1x1 %.1f us each\n", (now_ms() - t) * 1000 / 200);
	t = now_ms();
	for (i = 0; i < 200; i++)
		glFinish();
	out("TIME glFinish %.1f us each\n", (now_ms() - t) * 1000 / 200);

	if (!fp || frames <= 0)
		return;
	/* a stand-in for a light's interactions: QUADS overlapping quads with the
	 * interaction programs, vertices streamed into a VBO each frame (as the frame-temp
	 * vertex cache does), additive blending, then a swap */
	v = malloc(sizeof(struct ivert) * 4 * QUADS);
	{
		GLuint b;
		pGenBuffersARB(1, &b);
		pBindBufferARB(GL_ARRAY_BUFFER_ARB, b);
	}
	t = now_ms();
	for (f = 0; f < frames; f++) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		for (i = 0; i < QUADS; i++) {
			float o = (float)((i * 7 + f) % 32);
			fill_quad(v + 4 * i, o, o, W - 32 + o, H - 32 + o, 0, 0, 1);
		}
		pBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(struct ivert) * 4 * QUADS, v, GL_STREAM_DRAW_ARB);
		bind_interaction(vp);
		glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
		for (i = 0; i < QUADS; i++)
			draw_ivert((const struct ivert *)(sizeof(struct ivert) * 4 * i));
		glDisable(GL_BLEND);
		unbind_interaction();
		SDL_GL_SwapWindow(win);
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))
				;
		}
	}
	t = now_ms() - t;
	pBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	out("TIME bench %d frames of %d interaction quads at %dx%d: %.1f fps (%.2f ms a frame)\n",
	    frames, QUADS, W, H, frames * 1000.0 / t, t / frames);
	free(v);
}

int main(int argc, char **argv)
{
	SDL_GLContext ctx;
	int i, frames = 300;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-bench") && i + 1 < argc) frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-nocanary")) no_canary = 1;
		else if (!strcmp(argv[i], "-w") && i + 1 < argc) W = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-h") && i + 1 < argc) H = atoi(argv[++i]);
	}
	out("d3gltest 1 (%s)\n", __DATE__);
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		out("FAIL sdl-init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	win = SDL_CreateWindow("d3gltest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL);
	if (!win) {
		out("FAIL sdl-window: %s\n", SDL_GetError());
		return 1;
	}
	ctx = SDL_GL_CreateContext(win);
	if (!ctx) {
		out("FAIL gl-context: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetSwapInterval(0);
	glViewport(0, 0, W, H);
	gl_errors(NULL);

	t_strings();
	load_procs();
	t_canary("start");
	t_programs();
	t_canary("programs");
	t_vbo();
	t_canary("vbo");
	t_stencil();
	t_depth_bounds();
	t_s3tc();
	t_fixed();
	t_timing(frames);

	out("RESULT %d passed, %d failed, %d skipped\n", npass, nfail, nskip);
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return nfail ? 1 : 0;
}
