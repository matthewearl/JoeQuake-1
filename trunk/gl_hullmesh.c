#include "quakedef.h"

static vec3_t player_bbox_mins = {-16, -16, -24};
static vec3_t player_bbox_maxs = {16, 16, 32};

static hull_vertex_t cube_vertices[] = {
	{{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	{{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	{{0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

	{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
	{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
	{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
	{{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

	{{0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
	{{0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
	{{0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
	{{0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},

	{{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

	{{0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

	{{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
	{{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
	{{1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
	{{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
};

static int cube_indices[] = {
	// face quads
	3, 2, 1, 0,
	7, 6, 5, 4,
	11, 10, 9, 8,
	15, 14, 13, 12,
	19, 18, 17, 16,
	23, 22, 21, 20,

	// edges
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	8, 9, 9, 10, 10, 11, 11, 8,
	12, 13, 13, 14, 14, 15, 15, 12,
	16, 17, 17, 18, 18, 19, 19, 16,
	20, 21, 21, 22, 22, 23, 23, 20,
};

static GLuint hull_vbo = 0;
static GLuint hull_ibo = 0;
static GLuint cube_vbo = 0;
static GLuint cube_ibo = 0;
static const GLuint position_attr = 0;
static const GLuint normal_attr = 1;
static GLuint hull_program = 0;
static GLuint is_line_loc;

static void checkGlError (void)
{
	GLenum err = glGetError();

	if (err != GL_NO_ERROR)
		Sys_Error("GL error: %d\n", err);
}

void GlHullMesh_CreateShaders (void)
{
	const glsl_attrib_binding_t bindings[] = {
		{"position", position_attr},
		{"normal", normal_attr},
	};

	const GLchar *vert_source =
		"#version 130\n"
		"\n"
		"uniform int is_line;\n"
		"\n"
		"attribute vec3 position;\n"
		"attribute vec3 normal;\n"
		"\n"
		"varying vec3 frag_normal;\n"
		"varying vec3 frag_normal_c;\n"
		"\n"
		"out vec3 interp_position;\n"
		"\n"
		"void main() {\n"
		"    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n"
		"    gl_Position.z -= 5e-2 * is_line;"
		"    frag_normal = normal;\n"
		"    frag_normal_c = (gl_ModelViewMatrix * vec4(normal, 0.0)).xyz;\n"
		"    interp_position = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;\n"
		"}\n";

	const GLchar *frag_source =
		"#version 130\n"
		"\n"
		"uniform int is_line;\n"
		"\n"
		"varying vec3 frag_normal;\n"
		"varying vec3 frag_normal_c;\n"
		"\n"
		"in vec3 interp_position;\n"
		"\n"
		"void main() {\n"
		"    vec3 color = frag_normal * 0.25 + 0.75;\n"
		"    vec3 light_dir = normalize(-interp_position);\n"
		"    color *= 0.25 + 0.75 * dot(light_dir, frag_normal_c);\n"
		"    gl_FragColor = vec4(color * (1 - is_line), 1.0 - 0.8 * is_line);\n"
		"}\n";

	hull_program = GL_CreateProgram(vert_source, frag_source, 0, NULL);

	if (hull_program == 0)
		Sys_Error("Could not compile program");

	is_line_loc = GL_GetUniformLocation(&hull_program, "is_line");
}

static void LoadBufferData(
	GLuint *vbo, GLuint *ibo, hull_vertex_t *vertices,
	int *indices, int num_vertices, int num_indices)
{
	qglDeleteBuffers(1, vbo);
	qglGenBuffers(1, vbo);
	GL_BindBuffer(GL_ARRAY_BUFFER, *vbo);
	qglBufferData(GL_ARRAY_BUFFER,
					sizeof(hull_vertex_t) * num_vertices,
					vertices,
					GL_STATIC_DRAW);

	qglDeleteBuffers(1, ibo);
	qglGenBuffers(1, ibo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo);
	qglBufferData(GL_ELEMENT_ARRAY_BUFFER,
					sizeof(int) * num_indices,
					indices,
					GL_STATIC_DRAW);
}

void GlHullMesh_BuildVertexBuffer (void)
{
	hull_vertex_t *vertices;
	int *indices;
	int num_vertices, num_indices;

	// Convert the hull to a mesh and load it into buffers.
	HullMesh_MakeVertexArray(1,
							 &vertices, &num_vertices,
							 &indices, &num_indices);
	LoadBufferData(&hull_vbo, &hull_ibo, vertices, indices, num_vertices, num_indices);
	free(vertices);
	free(indices);

	// Load cube vertices/indices.
	LoadBufferData(&cube_vbo, &cube_ibo, cube_vertices, cube_indices,
					countof(cube_vertices), countof(cube_indices));
}

static void SetupFaces (GLuint vbo, GLuint ibo)
{
	if (hull_program == 0)
		Sys_Error("Hull program not compiled yet");
	qglUseProgram(hull_program);

	GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

	qglEnableVertexAttribArray(position_attr);
	qglEnableVertexAttribArray(normal_attr);

	qglVertexAttribPointer(position_attr, 3, GL_FLOAT, GL_FALSE,
							sizeof(hull_vertex_t),
							(void *)offsetof(hull_vertex_t, position));
	qglVertexAttribPointer(normal_attr, 3, GL_FLOAT, GL_FALSE,
							sizeof(hull_vertex_t),
							(void *)offsetof(hull_vertex_t, normal));

	qglUniform1i(is_line_loc, 0);
}

static void SetupEdges (void)
{
	qglUniform1i(is_line_loc, 1);
	glEnable(GL_BLEND);
}

static void FinishDraw (void)
{
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

	qglDisableVertexAttribArray(normal_attr);
	qglDisableVertexAttribArray(position_attr);

	qglUseProgram(0);

	checkGlError();
}

static void DrawBrushModel (model_t *model)
{
	SetupFaces(hull_vbo, hull_ibo);
	glDrawElements(GL_TRIANGLES, model->hullmesh_count, GL_UNSIGNED_INT,
					(void *)(sizeof(int) * model->hullmesh_start));
	SetupEdges();
	glDrawElements(GL_LINES, model->hullmesh_line_count, GL_UNSIGNED_INT,
					(void *)(sizeof(int) * model->hullmesh_line_start));
	FinishDraw();
}

void GL_PolygonOffset(int offset);
static void DrawCross(void)
{
	float line_length = 8;
	float diag_line_length = line_length / sqrt(3);

	glColor4f(0, 0, 0, 1);
	glCullFace(GL_FRONT);
	glPolygonMode(GL_BACK, GL_LINE);
	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);
	GL_PolygonOffset(-0.7);
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_LINES);

	glVertex3f(-line_length, 0.0f, 0.0f);
	glVertex3f(line_length, 0.0f, 0.0f);

	glVertex3f(0.0f, -line_length, 0.0f);
	glVertex3f(0.0f, line_length, 0.0f);

	glVertex3f(0.0f, 0.0f, -line_length);
	glVertex3f(0.0f, 0.0f, line_length);

	glVertex3f(-diag_line_length, -diag_line_length, -diag_line_length);
	glVertex3f(diag_line_length, diag_line_length, diag_line_length);

	glVertex3f(-diag_line_length, diag_line_length, -diag_line_length);
	glVertex3f(diag_line_length, -diag_line_length, diag_line_length);

	glVertex3f(diag_line_length, -diag_line_length, -diag_line_length);
	glVertex3f(-diag_line_length, diag_line_length, diag_line_length);

	glVertex3f(diag_line_length, diag_line_length, -diag_line_length);
	glVertex3f(-diag_line_length, -diag_line_length, diag_line_length);

	glEnd();

	glColor4f(1, 1, 1, 1);
	GL_PolygonOffset(0);
	glPolygonMode(GL_BACK, GL_FILL);
	glDisable(GL_LINE_SMOOTH);
	glCullFace(GL_BACK);
	glEnable(GL_TEXTURE_2D);
}


static void DrawBox(vec3_t mins, vec3_t maxs)
{
	vec3_t scale;

	VectorSubtract(maxs, mins, scale);

	glPushMatrix();
	glTranslatef(mins[0], mins[1], mins[2]);
	glScalef(scale[0], scale[1], scale[2]);

	SetupFaces(cube_vbo, cube_ibo);
	glDrawElements(GL_QUADS, 24, GL_UNSIGNED_INT, (void *)0);
	SetupEdges();
	glDrawElements(GL_LINES, 48, GL_UNSIGNED_INT, (void *)(sizeof(int) * 24));
	FinishDraw();

	glPopMatrix();
}

void GlHullMesh_DrawEntity (entity_t *ent)
{
	bbox_cat_t bbox_cat;
	vec3_t mins, maxs;

	glPushMatrix();
	glTranslatef(ent->origin[0], ent->origin[1], ent->origin[2]);

	if (&cl_entities[cl.viewentity] == ent)
	{
		DrawCross();
	}
	else if (ent->model->hullmesh_start != -1)
	{
		DrawBrushModel(ent->model);
	}
	else if (R_BboxForEnt(ent, mins, maxs, &bbox_cat))
	{
		if (bbox_cat != BBOX_CAT_PICKUP)
		{
			VectorSubtract(mins, player_bbox_maxs, mins);
			VectorSubtract(maxs, player_bbox_mins, maxs);
			DrawBox(mins, maxs);
		}
	}

	glPopMatrix();
}
