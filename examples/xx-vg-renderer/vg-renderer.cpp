/*
 * Copyright 2011-2022 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

//
// Copyright (c) 2022 Jim Drygiannakis
//

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"
#include <vg/vg.h>

namespace
{
struct Demo
{
	enum Enum : uint32_t
	{
		BouncingEllipse = 0,
		ChessBoard = 1,
	};
};

static const char* kDemoTitle[] = {
	"Bouncing Ellipse",
	"Chess Board"
};

vg::FontHandle createFont(vg::Context* _ctx, const char* _name, const char* _filePath)
{
	uint32_t size;
	void* data = load(_filePath, &size);
	if (NULL == data) {
		return VG_INVALID_HANDLE;
	}

	return vg::createFont(_ctx, _name, (uint8_t*)data, size, 0);
}

class ExampleVGRenderer : public entry::AppI
{
public:
	ExampleVGRenderer(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_vgCtx(NULL)
		, m_SelectedDemo(Demo::BouncingEllipse)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_TEXT;
		m_reset  = BGFX_RESET_VSYNC;

		// NOTE: D3D11 renderer requires USE_D3D11_STAGING_BUFFER to be set to 1 in renderer_d3d11.h
		// otherwise there is a bug which causes flickering/invalid triangles to appear from time to time.
		// bgfx::RendererType::OpenGL doesn't have this issue.
		bgfx::Init init;
		init.type     = args.m_type;
		init.vendorId = args.m_pciId;
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		// Sequential view mode is required by vg-renderer.
		bgfx::setViewMode(0, bgfx::ViewMode::Sequential);

		imguiCreate();

		m_vgCtx = vg::createContext(&m_vgAllocator, NULL);
		if (!m_vgCtx) {
			bx::debugPrintf("Failed to create vg-renderer context.\n");
			return;
		}

		m_SansFontHandle = createFont(m_vgCtx, "sans", "font/roboto-regular.ttf");
		if (!vg::isValid(m_SansFontHandle)) {
			bx::debugPrintf("Failed to load font.\n");
		}
	}

	virtual int shutdown() override
	{
		// NOTE: bgfx::frame() should be called (at least?) twice before calling vg::destroyContext()
		// in order to give bgfx a chance to call bgfx::Memory deallocation functions.
		bgfx::frame();
		bgfx::frame();
		vg::destroyContext(m_vgCtx);

		imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT   : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT  : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);
			showDemoDialog();

			imguiEndFrame();

			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			vg::begin(m_vgCtx, 0, uint16_t(m_width), uint16_t(m_height), 1.0f);
			{
				const float dt = 1.0f / 60.0f;

				if (m_SelectedDemo == Demo::BouncingEllipse) {
					static float pos[2] = { 100.0f, 100.0f };
					static float size[2] = { 200.0f, 100.0f };
					static float dir[2] = { 200.0f, -200.0f };
					static float sizeDelta[2] = { 50.0f, -30.0f };

					pos[0] += dir[0] * dt;
					pos[1] += dir[1] * dt;
					size[0] += sizeDelta[0] * dt;
					size[1] += sizeDelta[1] * dt;
					if (size[0] > 300.0f || size[0] < 100.0f) {
						sizeDelta[0] *= -1.0f;
					}
					if (size[1] > 300.0f || size[1] < 50.0f) {
						sizeDelta[1] *= -1.0f;
					}

					if (pos[0] - size[0] * 0.5f < 0.0f) {
						pos[0] = size[0] * 0.5f;
						dir[0] *= -1.0f;
					} else if (pos[0] + size[0] * 0.5f > (float)m_width) {
						pos[0] = (float)m_width - size[0] * 0.5f;
						dir[0] *= -1.0f;
					}

					if (pos[1] - size[1] * 0.5f < 0.0f) {
						pos[1] = size[1] * 0.5f;
						dir[1] *= -1.0f;
					} else if (pos[1] + size[1] * 0.5f > (float)m_height) {
						pos[1] = (float)m_height - size[1] * 0.5f;
						dir[1] *= -1.0f;
					}

					vg::beginPath(m_vgCtx);
					vg::ellipse(m_vgCtx, pos[0], pos[1], size[0] * 0.5f, size[1] * 0.5f);
					vg::fillPath(m_vgCtx, vg::Colors::Red, vg::FillFlags::Convex);
					vg::strokePath(m_vgCtx, vg::Colors::Black, 4.0f, vg::StrokeFlags::ButtMiterAA);

					vg::TextConfig txtCfg = vg::makeTextConfig(m_vgCtx, m_SansFontHandle, 20.0f, vg::TextAlign::MiddleCenter, vg::Colors::Black);
					vg::text(m_vgCtx, txtCfg, pos[0], pos[1], u8"Hello World\u2026", nullptr);
				} else {
					vg::TextConfig txtCfg = vg::makeTextConfig(m_vgCtx, m_SansFontHandle, 20.0f, vg::TextAlign::MiddleCenter, vg::Colors::White);
					vg::text(m_vgCtx, txtCfg, (float)(m_width / 2), (float)(m_height / 2), "TODO: Demo not implemented yet.", nullptr);
				}
			}
			vg::end(m_vgCtx);
			vg::frame(m_vgCtx);

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	void showDemoDialog()
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 300.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300.0f, 80.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("vg-renderer Demo")) {
			if (ImGui::BeginCombo("Demo", kDemoTitle[m_SelectedDemo])) {
				for (uint32_t i = 0; i < BX_COUNTOF(kDemoTitle); ++i) {
					const bool is_selected = (m_SelectedDemo == i);
					if (ImGui::Selectable(kDemoTitle[i], is_selected)) {
						m_SelectedDemo = (Demo::Enum)i;
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			ImGui::End();
		}
	}

	bx::DefaultAllocator m_vgAllocator;
	vg::Context* m_vgCtx;
	vg::FontHandle m_SansFontHandle;
	Demo::Enum m_SelectedDemo;

	entry::MouseState m_mouseState;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExampleVGRenderer
	, "xx-vg-renderer"
	, "vg-renderer demo."
	, "https://bkaradzic.github.io/bgfx/examples.html#vg-renderer"
	);
