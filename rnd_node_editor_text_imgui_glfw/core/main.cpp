
#include <array>

#include "GLAD/glad.h"

#include <iostream>
#include <mutex>
#include <thread>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

#include "RadeonProRender_v2.h"
#include "Math/mathutils.h"
#include "common.h"
#include "hrs_shader_manager.h"

#include "node_editor.hpp"

using namespace std;

int m_window_width_ = 1280;
int m_window_height_ = 800;

GLFWwindow* window;
RPRGarbageCollector g_gc;
rpr_context context = nullptr;
rpr_material_system materialSystem = nullptr;

// Radeon Buffers and Textures
rpr_framebuffer m_frame_buffer_ = nullptr;
rpr_framebuffer m_frame_buffer_2_ = nullptr;
GLuint m_program_;
GLuint m_texture_buffer_ = 0;
GLuint m_vertex_buffer_id_ = 0;
GLuint m_index_buffer_id_ = 0;
GLuint m_up_buffer_ = 0;
ShaderManager m_shader_manager_;

bool m_thread_running_ = false;
bool m_is_dirty_;
std::shared_ptr<float> m_fb_data_ = nullptr;

std::thread m_render_thread_;
std::mutex renderMutex;

int m_min_samples_ = 4;
int m_max_samples_ = 128;
int m_sample_count_ = 0;
int m_batch_size_ = 0;

inline static float last_progress = -1.0f;
inline static bool has_started = false;
inline static bool is_options_changed = false;
inline static std::chrono::high_resolution_clock::time_point start_time;
inline static std::chrono::high_resolution_clock::time_point end_time;
long long total_seconds;
long long hours;
long long minutes;
long long seconds;
long long milliseconds;
long long m_chrono_time_;
static long long duration = 0;
bool options_changed = false;

ImVec2 img_size;
float aspect_ratio_viewer;
ImVec2 stored_image_position_;

class Render_Progress_Callback
{
public:

	struct Update
	{
		Update() : m_hasUpdate(0), m_done(0), m_aborted(0), render_ready_(false), m_camUpdated(0), m_progress(0.0f) {}

		volatile int m_hasUpdate;
		volatile int m_done;
		volatile int m_aborted;
		bool render_ready_;
		int m_camUpdated;
		float m_progress;

		void clear()
		{
			m_hasUpdate = m_done = m_aborted = m_camUpdated = 0;
		}
	};

	static void notifyUpdate(float x, void* userData)
	{
		auto update = static_cast<Update*>(userData);
		update->m_hasUpdate = 1;
		update->m_progress = x;
	}
};

Render_Progress_Callback::Update render_progress_callback;

const auto invalid_time = std::chrono::time_point<std::chrono::high_resolution_clock>::max();
int benchmark_number_of_render_iteration = 0;
auto benchmark_start = invalid_time;

void render_job(rpr_context ctxt, Render_Progress_Callback::Update* update)
{
	renderMutex.lock();
	CHECK(rprContextRender(ctxt));
	//rprContextRender(ctxt);
	update->m_done = 1;
	renderMutex.unlock();
}

// OpenGL	
void opengl_init()
{
	// Opengl / GLFW

	if (!glfwInit())
	{
		cout << "Error: GLFW failed to initialize" << endl;
		exit(-1);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	glfwSetErrorCallback([](int error, const char* description)
		{
			cout << "Error: " << description << endl;
		});

	window = glfwCreateWindow(m_window_width_, m_window_height_, "Node Editor", nullptr, nullptr);

	if (!window)
	{
		cout << "Error: GLFW failed to create window" << endl;
		glfwTerminate();
		exit(-1);
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Error: Failed to initialize GLAD" << std::endl;
		exit(-1);
	}

}
void opengl_render()
{
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void opengl_post_render()
{
	glfwPollEvents();
	glfwSwapBuffers(window);
}
void opengl_cleanup()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

// Imgui
void imgui_init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

	ImGui::StyleColorsDark();               // Apply the dark theme

	ImGuiStyle& style = ImGui::GetStyle();  // Get the style structure
	ImVec4* colors = style.Colors;          // Get the colors array

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		// Background

		colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.105f, 0.11f, 1.0f);

		// Headers

		colors[ImGuiCol_Header] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

		// Headers Hovered

		colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);

		// Headers Active

		colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Buttons

		colors[ImGuiCol_Button] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

		// Buttons Hovered

		colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);

		// Buttons Active

		colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Frame Background

		colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

		// Frame Background Hovered

		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);

		// Frame Background Active

		colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Tabs

		colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Tabs Hovered

		colors[ImGuiCol_TabHovered] = ImVec4(0.39f, 0.4f, 0.41f, 1.0f);

		// Tabs Active

		colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

		// Title

		colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Title Active

		colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Title Text

		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// List Box Background

		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);


		// Imgui style setup

		//style->WindowRounding = 0.0f;
	}

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");
}
void imgui_init_render()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_AlwaysAutoResize;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("Horus_MainDockspace", nullptr, window_flags);

	ImGui::PopStyleVar(3);
	ImGuiID dockspace_id = ImGui::GetID("Horus_MainDockspace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	ImGui::End();
}
void imgui_post_render()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}
void imgui_cleanup()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();
}

// Radeon
std::tuple<GLuint, GLuint, GLuint> get_shader_variables(GLuint program, const char* texture_name, const char* position_name, const char* texcoord_name)
{
	GLuint texture_loc = glGetUniformLocation(program, texture_name);
	GLuint position_attr_id = glGetAttribLocation(program, position_name);
	GLuint texcoord_attr_id = glGetAttribLocation(program, texcoord_name);

	return std::make_tuple(texture_loc, position_attr_id, texcoord_attr_id);
}
float get_render_progress()
{
	int maxSpp = m_max_samples_;
	float progress = maxSpp <= 0 ? 0.0f : m_sample_count_ * 100.0f / maxSpp;

	if (progress >= 100.0f)
	{
		m_is_dirty_ = false;
	}

	return progress;
}
int get_min_samples()
{
	return m_min_samples_;
}
int get_max_samples()
{
	return m_max_samples_;
}
int set_min_samples(int min_samples)
{
	m_min_samples_ = min_samples;
	return m_min_samples_;
}
int set_max_samples(int max_samples)
{
	m_max_samples_ = max_samples;
	return m_max_samples_;
}
int get_sample_count()
{
	return m_sample_count_;
}
bool get_is_dirty()
{
	return m_is_dirty_;
}
bool set_is_dirty(bool is_dirty)
{
	m_is_dirty_ = is_dirty;
	return m_is_dirty_;
}
void set_sample_count(int sample_count)
{
	m_sample_count_ = sample_count;
}
GLuint get_texture_buffer()
{
	return m_texture_buffer_;
}
rpr_framebuffer get_frame_buffer()
{
	return m_frame_buffer_;
}
rpr_framebuffer get_frame_buffer_resolved()
{
	return m_frame_buffer_2_;
}
RadeonProRender::float2 set_window_size(int width, int height)
{
	m_window_width_ = width;
	m_window_height_ = height;

	return RadeonProRender::float2(static_cast<float>(m_window_width_), static_cast<float>(m_window_height_));
}

void reset_buffer()
{
	options_changed = true;
	set_is_dirty(true);
	set_sample_count(1);
	CHECK(rprFrameBufferClear(get_frame_buffer()));
}

void radeon_init()
{
	rpr_int status = RPR_SUCCESS;

	rpr_int pluginID = rprRegisterPlugin(RPR_PLUGIN_FILE_NAME);
	CHECK_NE(pluginID, -1);

	rpr_int plugins[] = { pluginID };
	size_t numPlugins = sizeof(plugins) / sizeof(plugins[0]);

	status = rprCreateContext(RPR_API_VERSION, plugins, numPlugins,
		RPR_CREATION_FLAGS_ENABLE_GL_INTEROP | RPR_CREATION_FLAGS_ENABLE_GPU0 |
		RPR_CREATION_FLAGS_ENABLE_GPU1 | RPR_CREATION_FLAGS_ENABLE_CPU,
		g_contextProperties, nullptr, &context);

	CHECK(status);

	CHECK(rprContextSetActivePlugin(context, plugins[0]));
	CHECK(rprContextCreateMaterialSystem(context, 0, &materialSystem));

	// Scene
	rpr_scene scene = nullptr;
	CHECK(rprContextCreateScene(context, &scene));
	CHECK(rprContextSetScene(context, scene));

	// Camera
	rpr_camera camera = nullptr;
	CHECK(rprContextCreateCamera(context, &camera));
	RadeonProRender::float3 eyePos(4, 4.0, 15);
	CHECK(rprCameraLookAt(camera, eyePos.x, eyePos.y, eyePos.z, 1.5, 0, 0, 0, 1, 0));
	CHECK(rprSceneSetCamera(scene, camera));

	// Create env light
	CHECK(CreateNatureEnvLight(context, scene, g_gc, 0.8f));

	{
		// Define the teapots list used in the scene
		struct TEAPOT_DEF
		{
			TEAPOT_DEF(float x_, float z_, float rot_, float r_, float g_, float b_)
			{
				x = x_;
				z = z_;
				rot = rot_;
				r = r_ / 255.0f;
				g = g_ / 255.0f;
				b = b_ / 255.0f;
				shape = nullptr;
				mat = nullptr;
			}

			float x;
			float z;
			float rot;
			float r;
			float g;
			float b;
			rpr_shape shape;
			rpr_material_node mat;
		};
		std::vector<TEAPOT_DEF> posList;
		posList.push_back(TEAPOT_DEF(5.0f, 2.0f, 1.6f, 122, 63, 0)); // brown
		posList.push_back(TEAPOT_DEF(-5.0f, 3.0f, 5.6f, 122, 0, 14));
		posList.push_back(TEAPOT_DEF(0.0f, -3.0f, 3.2f, 119, 0, 93));
		posList.push_back(TEAPOT_DEF(1.0f, +3.0f, 1.2f, 7, 0, 119));
		posList.push_back(TEAPOT_DEF(3.0f, +9.0f, -1.7f, 0, 59, 119));
		posList.push_back(TEAPOT_DEF(-6.0f, +12.0f, 2.2f, 0, 119, 99));
		posList.push_back(TEAPOT_DEF(9.0f, -6.0f, 4.8f, 0, 119, 1)); // green
		posList.push_back(TEAPOT_DEF(9.0f, 7.0f, 2.5f, 219, 170, 0)); // yellow
		posList.push_back(TEAPOT_DEF(-9.0f, -7.0f, 5.8f, 112, 216, 202));


		// create teapots
		int i = 0;
		for (const auto iShape : posList)
		{
			rpr_shape teapot01 = nullptr;

			if (i == 0)
			{
				// create from OBJ for the first teapot
				teapot01 = ImportOBJ("Resources/Meshes/teapot.obj", scene, context);
			}
			else
			{
				// other teapots will be instances of the first one.
				CHECK(rprContextCreateInstance(context, posList[0].shape, &teapot01));
				CHECK(rprSceneAttachShape(scene, teapot01));
			}

			// random transforms of teapot on the floor :

			RadeonProRender::matrix m;

			if (i % 4 == 0)
				m = RadeonProRender::translation(RadeonProRender::float3(iShape.x, 0.0f, iShape.z)) * RadeonProRender::rotation_y(iShape.rot)
				* RadeonProRender::rotation_x(MY_PI);

			if (i % 4 == 1)
				m = RadeonProRender::translation(RadeonProRender::float3(iShape.x, 0.0f, iShape.z)) * RadeonProRender::rotation_y(iShape.rot)
				* RadeonProRender::translation(RadeonProRender::float3(0, 2.65, 0))
				* RadeonProRender::rotation_x(MY_PI + 1.9f)
				* RadeonProRender::rotation_y(0.45);


			if (i % 4 == 2)
				m = RadeonProRender::translation(RadeonProRender::float3(iShape.x, 0.0f, iShape.z)) * RadeonProRender::rotation_y(iShape.rot)
				* RadeonProRender::translation(RadeonProRender::float3(0, 2.65, 0))
				* RadeonProRender::rotation_x(MY_PI + 1.9f)
				* RadeonProRender::rotation_y(-0.57);


			if (i % 4 == 3)
				m = RadeonProRender::translation(RadeonProRender::float3(iShape.x, 0.0f, iShape.z)) * RadeonProRender::rotation_y(iShape.rot)
				* RadeonProRender::translation(RadeonProRender::float3(0, 3.38, 0))
				* RadeonProRender::rotation_x(+0.42f)
				* RadeonProRender::rotation_z(-0.20f)
				;

			CHECK(rprShapeSetTransform(teapot01, RPR_TRUE, &m.m00));

			posList[i].shape = teapot01;

			i++;
		}




	}

	// create the floor
	CHECK(CreateAMDFloor(context, scene, materialSystem, g_gc, 1.0f, 1.0f));

	CHECK(rprContextSetParameterByKey1f(context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));

	rpr_framebuffer_desc desc = { static_cast<unsigned int>(m_window_width_), static_cast<unsigned int>(m_window_height_) };
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	CHECK(rprContextCreateFrameBuffer(context, fmt, &desc, &m_frame_buffer_));
	CHECK(rprContextCreateFrameBuffer(context, fmt, &desc, &m_frame_buffer_2_));

	CHECK(rprContextSetAOV(context, RPR_AOV_COLOR, m_frame_buffer_));

	CHECK(rprContextSetParameterByKeyPtr(context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, (void*)Render_Progress_Callback::notifyUpdate));
	CHECK(rprContextSetParameterByKeyPtr(context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, &render_progress_callback));

	CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_ITERATIONS, 1));

	std::thread renderThread([&]() {
		CHECK(rprContextRender(context));
		});
	renderThread.join();

	CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_ITERATIONS, m_batch_size_));

	m_fb_data_ = std::shared_ptr<float>(new float[m_window_width_ * m_window_height_ * 4], std::default_delete<float[]>());

}
bool radeon_init_pre_render(int width, int height)
{
	m_window_width_ = width;
	m_window_height_ = height;
	m_is_dirty_ = true;

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glCullFace(GL_NONE);
	glDisable(GL_DEPTH_TEST);

	glViewport(0, 0, m_window_width_, m_window_height_);

	glEnable(GL_TEXTURE_2D);

	glGenBuffers(1, &m_vertex_buffer_id_);
	glGenBuffers(1, &m_index_buffer_id_);

	glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer_id_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_id_);


	constexpr std::array<float, 20> quadVertexData = { -1, -1, 0.5, 0, 0, 1, -1, 0.5, 1, 0, 1, 1, 0.5, 1, 1, -1, 1, 0.5, 0, 1 };
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertexData), quadVertexData.data(), GL_STATIC_DRAW);

	constexpr std::array<GLshort, 6> quadIndexData = { 0, 1, 3, 3, 1, 2 };
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndexData), quadIndexData.data(), GL_STATIC_DRAW);


	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenTextures(1, &m_texture_buffer_);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture_buffer_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_window_width_, m_window_height_, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	set_window_size(m_window_width_, m_window_height_);

	return true;
}
void radeon_init_render()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer_id_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_id_);

	m_program_ = m_shader_manager_.get_program("core/shaders/shader");
	auto [texture_location, position_location, texcoord_location] = get_shader_variables(m_program_, "g_Texture", "inPosition", "inTexcoord");

	glUseProgram(m_program_);
	glUniform1i(texture_location, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture_buffer_);

	glVertexAttribPointer(position_location, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, nullptr);
	glVertexAttribPointer(texcoord_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));

	glEnableVertexAttribArray(position_location);
	glEnableVertexAttribArray(texcoord_location);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

	glDisableVertexAttribArray(position_location);
	glDisableVertexAttribArray(texcoord_location);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);
}
void radeon_post_render()
{
	
}
void radeon_cleanup()
{
	glDeleteTextures(1, &m_texture_buffer_);

	CHECK(rprObjectDelete(materialSystem)); materialSystem = nullptr;
	CHECK(rprObjectDelete(m_frame_buffer_)); m_frame_buffer_ = nullptr;
	CHECK(rprObjectDelete(m_frame_buffer_2_)); m_frame_buffer_2_ = nullptr;

	g_gc.GCClean();

	rprContextClearMemory(context);
	CheckNoLeak(context);
	CHECK(rprObjectDelete(context)); context = nullptr;
}
void radeon_create_framebuffer(int width, int height)
{
	m_window_width_ = width;
	m_window_height_ = height;

	glDeleteTextures(1, &m_texture_buffer_);

	CHECK(rprObjectDelete(m_frame_buffer_));
	CHECK(rprObjectDelete(m_frame_buffer_2_));

	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	rpr_framebuffer_desc desc = { static_cast<unsigned int>(m_window_width_), static_cast<unsigned int>(m_window_height_) };

	CHECK(rprContextCreateFrameBuffer(context, fmt, &desc, &m_frame_buffer_));
	CHECK(rprContextCreateFrameBuffer(context, fmt, &desc, &m_frame_buffer_2_));
}
void radeon_render_engine()
{
	const auto timeUpdateStarts = std::chrono::high_resolution_clock::now();

	if (benchmark_start == invalid_time)
		benchmark_start = timeUpdateStarts;
	if (benchmark_number_of_render_iteration >= 100)
	{
		double elapsed_time_ms = std::chrono::duration<double, std::milli>(timeUpdateStarts - benchmark_start).
			count();
		double renderPerSecond = static_cast<double>(benchmark_number_of_render_iteration) * 1000.0 /
			elapsed_time_ms;
		std::cout << renderPerSecond << " iterations per second." << std::endl;
		benchmark_number_of_render_iteration = 0;
		benchmark_start = timeUpdateStarts;
	}

	render_progress_callback.clear();

	if (m_is_dirty_)
	{
		m_render_thread_ = std::thread(render_job, context, &render_progress_callback);

		m_thread_running_ = true;
	}

	while (!render_progress_callback.m_done)
	{
		if (!m_is_dirty_ && m_max_samples_ != -1 && m_sample_count_ >= m_max_samples_)
		{
			break;
		}

		if (render_progress_callback.m_hasUpdate)
		{
			renderMutex.lock();

			m_sample_count_++;

			rpr_int status = rprContextResolveFrameBuffer(context, m_frame_buffer_, m_frame_buffer_2_, false);
			if (status != RPR_SUCCESS) {
				std::cout << "RPR Error: " << status << std::endl;
			}

			size_t framebuffer_size = 0;
			CHECK(rprFrameBufferGetInfo(m_frame_buffer_2_, RPR_FRAMEBUFFER_DATA, 0, NULL, &framebuffer_size));

			if (framebuffer_size != m_window_width_ * m_window_height_ * 4 * sizeof(float))
			{
				CHECK(RPR_ERROR_INTERNAL_ERROR);
			}

			CHECK(rprFrameBufferGetInfo(m_frame_buffer_2_, RPR_FRAMEBUFFER_DATA, framebuffer_size, m_fb_data_.get(), NULL));

			glBindTexture(GL_TEXTURE_2D, m_texture_buffer_);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_window_width_, m_window_height_, GL_RGBA, GL_FLOAT,
				static_cast<const GLvoid*>(m_fb_data_.get()));

			glBindTexture(GL_TEXTURE_2D, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_buffer_, 0);

			render_progress_callback.m_hasUpdate = false;

			renderMutex.unlock();

			break;

		}
	}

	if (m_thread_running_)
	{
		m_render_thread_.join();
		m_thread_running_ = false;
	}

	get_render_progress();

	benchmark_number_of_render_iteration += m_batch_size_;

}
void radeon_resize_render(int width, int height)
{
	m_window_width_ = width;
	m_window_height_ = height;

	glViewport(0, 0, m_window_width_, m_window_height_);

	m_fb_data_ = nullptr;

	radeon_create_framebuffer(m_window_width_, m_window_height_);

	CHECK(rprContextSetAOV(context, RPR_AOV_COLOR, m_frame_buffer_));

	m_fb_data_ = std::shared_ptr<float>(new float[m_window_width_ * m_window_height_ * 4], std::default_delete<float[]>());

	radeon_init_pre_render(m_window_width_, m_window_height_);

	CHECK(rprContextResolveFrameBuffer(context, m_frame_buffer_, m_frame_buffer_2_, false));
	size_t fb_size = 0;
	CHECK(rprFrameBufferGetInfo(m_frame_buffer_2_, RPR_FRAMEBUFFER_DATA, 0, nullptr, &fb_size));

	set_window_size(m_window_width_, m_window_height_);
	m_sample_count_ = 1;
	
}


// UI
void viewer()
{
	static bool isResizable = false;
	static ImVec2 lastSize = ImVec2(0, 0);
	static int customX = 800;
	static int customY = 600;
	static int lastCustomX = customX;
	static int lastCustomY = customY;
	static float offsetX_adjust = 0.0f;
	static float offsetY_adjust = 0.0f;

	static ImVec2 viewer_window_pos;
	static ImVec2 viewer_window_size;

	ImGuiIO& io = ImGui::GetIO();
	bool isLeftAltPressed = io.KeyAlt;

	ImGuiWindowFlags window_flags = 0;

	if (isLeftAltPressed) {
		window_flags |= ImGuiWindowFlags_NoMove;
	}

	const char* items[] = { "800x600", "1024x768", "1280x536", "1280x720", "1920x1080", "1920x803", "2048x858" };
	static int item_current = 0;

	if (ImGui::Begin("Viewer", nullptr, ImGuiWindowFlags_MenuBar | window_flags))
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::Text("Size : ");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(120);
			ImGui::DragInt("X", &customX);
			ImGui::SameLine();

			ImGui::Text(" x ");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(120);
			ImGui::DragInt("Y", &customY);
			ImGui::SameLine();

			ImGui::SetNextItemWidth(150);
			if (ImGui::Combo("Predefined Sizes", &item_current, items, IM_ARRAYSIZE(items)))
			{
				sscanf_s(items[item_current], "%dx%d", &customX, &customY);
			}

			ImGui::SameLine();

			ImGui::Checkbox("Resizable", &isResizable);

			ImGui::SameLine();

			ImGui::SameLine();
			// progress bar
			float progress = get_render_progress();
			int max_samples = get_max_samples();
			int current_samples = static_cast<int>(progress / 100.0f * max_samples);
			ImGui::SetNextItemWidth(100);
			ImGui::Text("Progress : ");
			ImGui::SetNextItemWidth(250);
			char overlayText[32];
			sprintf_s(overlayText, "%d sur %d", current_samples, max_samples);
			ImGui::ProgressBar(progress / 100.0f, ImVec2(0.0f, 0.0f), overlayText);
			bool isRenderComplete = (progress >= 100.0f);
			ImGui::EndMenuBar();



			if (progress <= m_min_samples_ && !has_started)
			{
				start_time = std::chrono::high_resolution_clock::now();
				has_started = true;
				options_changed = false;
			}

			if (progress >= 99.f && has_started)
			{
				auto end_time = std::chrono::high_resolution_clock::now();
				auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

				total_seconds = duration_ms / 1000;
				hours = total_seconds / 3600;
				minutes = (total_seconds % 3600) / 60;
				seconds = total_seconds % 60;
				milliseconds = duration_ms % 1000;



				m_chrono_time_ = duration;
				has_started = false;
			}



			if (isRenderComplete)
			{
				char timeString[100];
				snprintf(timeString, sizeof(timeString), "Rendering finish in : %lldh %lldm %llds %lldms", hours, minutes, seconds, milliseconds);




				ImVec2 overlay_pos = ImVec2(
					viewer_window_pos.x + viewer_window_size.x / 2.0f,
					viewer_window_pos.y + viewer_window_size.y - 30.0f
				);

				ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

				ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always, ImVec2(0.5f, 1.0f));

				ImGui::SetNextWindowBgAlpha(0.35f);
				if (ImGui::Begin("finish_render", NULL, overlay_flags))
				{
					ImGui::Separator();
					ImGui::Text("%s", timeString);
					ImGui::Separator();
				}
				ImGui::End();


			}
			last_progress = progress;
		}

		if (customX != lastCustomX || customY != lastCustomY)
		{
			radeon_resize_render(customX, customY);
			lastCustomX = customX;
			lastCustomY = customY;
		}

		GLuint TextureID = get_texture_buffer();
		ImVec2 m_viewer_size = ImGui::GetContentRegionAvail();



		if (isResizable)
		{
			if (m_viewer_size.x != lastSize.x || m_viewer_size.y != lastSize.y)
			{
				radeon_resize_render(static_cast<int>(m_viewer_size.x), static_cast<int>(m_viewer_size.y));
				lastSize = m_viewer_size;
			}

			ImGui::Image((void*)(intptr_t)TextureID, ImVec2(m_viewer_size.x, m_viewer_size.y));
		}
		else
		{
			float aspect_ratio_image = static_cast<float>(customX) / static_cast<float>(customY);
			aspect_ratio_viewer = aspect_ratio_image;

			// Conserve le ratio d'aspect
			if (m_viewer_size.x / m_viewer_size.y > aspect_ratio_image)
			{
				img_size.y = m_viewer_size.y;
				img_size.x = m_viewer_size.y * aspect_ratio_image;
			}
			else
			{
				img_size.x = m_viewer_size.x;
				img_size.y = m_viewer_size.x / aspect_ratio_image;
			}

			float offsetX = (m_viewer_size.x - img_size.x - 60) * 0.5f;
			float offsetY = (m_viewer_size.y - img_size.y) * 0.5f;

			ImGui::SetCursorPos(ImVec2(offsetX + 30, offsetY + 60));

			ImGui::Image((void*)(intptr_t)TextureID, img_size);

			float middleX = offsetX + 30 + img_size.x / 2.0f;
			float middleY = offsetY + 60 + img_size.y / 2.0f;

			stored_image_position_ = ImVec2(middleX, middleY);
		}

		viewer_window_pos = ImGui::GetWindowPos();
		viewer_window_size = ImGui::GetWindowSize();
	}

	ImGui::End();
}


int main()
{
	opengl_init();
	imgui_init();
	radeon_init();
	
	radeon_init_pre_render(m_window_width_, m_window_height_);



	while (!glfwWindowShouldClose(window))
	{
		opengl_render();
		imgui_init_render();
		radeon_init_render();

		radeon_render_engine();

		viewer();

		imgui_post_render();
		opengl_post_render();

	}

	imgui_cleanup();
	opengl_cleanup();

	return 0;

}