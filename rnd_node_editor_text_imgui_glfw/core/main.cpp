
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

#include "node_editor.hpp"
#include "radeon.hpp"

using namespace std;

int m_window_width_ = 1280;
int m_window_height_ = 800;

GLFWwindow* window;
RPRGarbageCollector g_gc;
rpr_context context = nullptr;
rpr_material_system materialSystem = nullptr;
rpr_framebuffer m_frame_buffer_ = nullptr;
rpr_framebuffer m_frame_buffer_2_ = nullptr;

int m_batch_size_ = 0;
std::shared_ptr<float> m_fb_data_ = nullptr;
std::mutex renderMutex;

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
void radeon_init_render()
{
	

}
void radeon_post_render()
{
	
}
void radeon_cleanup()
{
	CHECK(rprObjectDelete(materialSystem)); materialSystem = nullptr;
	CHECK(rprObjectDelete(m_frame_buffer_)); m_frame_buffer_ = nullptr;
	CHECK(rprObjectDelete(m_frame_buffer_2_)); m_frame_buffer_2_ = nullptr;

	g_gc.GCClean();

	rprContextClearMemory(context);
	CheckNoLeak(context);
	CHECK(rprObjectDelete(context)); context = nullptr;
}
void radeon_resize_render()
{
	
}



int main()
{
	opengl_init();
	imgui_init();
	radeon_init();
	





	while (!glfwWindowShouldClose(window))
	{
		opengl_render();
		imgui_init_render();












		imgui_post_render();
		opengl_post_render();
	}

	imgui_cleanup();
	opengl_cleanup();

	return 0;

}