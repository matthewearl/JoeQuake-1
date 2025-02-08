#include "quakedef.h"

static int num_indices;
static GLuint vbo = 0;
static GLuint ibo = 0;
static const GLuint position_attr = 0;
static const GLuint normal_attr = 1;
static GLuint hull_program = 0;

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
		"attribute vec3 position;\n"
		"attribute vec3 normal;\n"
		"\n"
		"varying vec3 frag_normal;\n"
		"\n"
		"void main() {\n"
		"    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n"
		"    frag_normal = normal;\n"
		"}\n";


	const GLchar *frag_source =
		"#version 130\n"
		"\n"
		"varying vec3 frag_normal;\n"
		"\n"
		"void main() {\n"
		"    vec3 color = frag_normal * 0.5 + 0.5;\n"
		"    gl_FragColor = vec4(color, 1.0);\n"
		"}\n";

	hull_program = GL_CreateProgram(vert_source, frag_source, 0, NULL);

	if (hull_program == 0)
		Sys_Error("Could not compile program");

}

void GlHullMesh_BuildVertexBuffer (void)
{
	hull_vertex_t *vertices;
	int num_vertices;
	int *indices;

	// Convert the hull to a mesh.
	HullMesh_MakeVertexArray(1,
							 &vertices, &num_vertices,
							 &indices, &num_indices);

	// Move the vertices into a vertex buffer.
	qglDeleteBuffers(1, &vbo);
	qglGenBuffers(1, &vbo);
	qglBindBuffer(GL_ARRAY_BUFFER, vbo);
	qglBufferData(GL_ARRAY_BUFFER,
					sizeof(hull_vertex_t) * num_vertices,
					vertices,
					GL_STATIC_DRAW);
	free(vertices);

	// Move the indices into an index buffer.
	qglDeleteBuffers(1, &ibo);
	qglGenBuffers(1, &ibo);
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	qglBufferData(GL_ELEMENT_ARRAY_BUFFER,
					sizeof(int) * num_indices,
					indices,
					GL_STATIC_DRAW);
	free(indices);
}

void GlHullMesh_Render (model_t *model)
{
	if (hull_program == 0)
		Sys_Error("Hull program not compiled yet");
	qglUseProgram(hull_program);

	GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

	qglEnableVertexAttribArray(position_attr);
	qglEnableVertexAttribArray(normal_attr);

	GL_BindBuffer(GL_ARRAY_BUFFER, vbo);
	qglVertexAttribPointer(position_attr, 3, GL_FLOAT, GL_FALSE,
							sizeof(hull_vertex_t),
							(void *)offsetof(hull_vertex_t, position));
	qglVertexAttribPointer(normal_attr, 3, GL_FLOAT, GL_FALSE,
							sizeof(hull_vertex_t),
							(void *)offsetof(hull_vertex_t, normal));

	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glDrawElements(GL_TRIANGLES, model->hullmesh_count, GL_UNSIGNED_INT,
					(void *)(sizeof(int) * model->hullmesh_start));

	qglDisableVertexAttribArray(normal_attr);
	qglDisableVertexAttribArray(position_attr);

	qglUseProgram(0);

	checkGlError();
}
