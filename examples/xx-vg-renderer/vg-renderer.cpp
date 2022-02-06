/*
 * Copyright 2011-2022 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

//
// Copyright (c) 2022 Jim Drygiannakis
// NanoVG demo code is copyright (c) 2013 Mikko Mononen memon@inside.org
// Paths for all the chess pieces have been taken from Wikimedia (https://commons.wikimedia.org/wiki/Category:SVG_chess_pieces)

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"
#include <bimg/decode.h>
#include <vg/vg.h>
#include <math.h>

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter");
#define BLENDISH_IMPLEMENTATION
#include "blendish.h"
BX_PRAGMA_DIAGNOSTIC_POP();

namespace
{
struct Demo
{
	enum Enum : uint32_t
	{
		BouncingEllipse = 0,
		NanoVGDemo,
		Chessboard
	};
};

static const char* kDemoTitle[] = {
	"Bouncing Ellipse",
	"NanoVG Demo",
	"Chessboard"
};

struct NanoVGDemoData
{
	vg::FontHandle fontNormal, fontBold, fontIcons, fontEmoji;
	vg::ImageHandle images[12];
};

struct ChessPiece
{
	enum Enum : uint32_t
	{
		King = 0,
		Queen,
		Rook,
		Bishop,
		Knight,
		Pawn
	};
};

struct ChessboardDemoData
{
	vg::CommandListHandle m_WhitePieces[6];
	vg::CommandListHandle m_BlackPieces[6];
	vg::FontHandle m_Font;
};

#define ICON_SEARCH 0x1F50D
#define ICON_CIRCLED_CROSS 0x2716
#define ICON_CHEVRON_RIGHT 0xE75E
#define ICON_CHECK 0x2713
#define ICON_LOGIN 0xE740
#define ICON_TRASH 0xE729

// Returns 1 if col.rgba is 0.0f,0.0f,0.0f,0.0f, 0 otherwise
int isBlack(vg::Color col)
{
	return col == vg::Colors::Transparent ? 1 : 0;
}

static char* cpToUTF8(int cp, char* str)
{
	int n = 0;
	if (cp < 0x80) n = 1;
	else if (cp < 0x800) n = 2;
	else if (cp < 0x10000) n = 3;
	else if (cp < 0x200000) n = 4;
	else if (cp < 0x4000000) n = 5;
	else if (cp <= 0x7fffffff) n = 6;

	str[n] = '\0';

	switch (n) {
	case 6: str[5] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x4000000; BX_FALLTHROUGH;
	case 5: str[4] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x200000;  BX_FALLTHROUGH;
	case 4: str[3] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x10000;   BX_FALLTHROUGH;
	case 3: str[2] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x800;     BX_FALLTHROUGH;
	case 2: str[1] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0xc0;      BX_FALLTHROUGH;
	case 1: str[0] = char(cp); break;
	}

	return str;
}

vg::FontHandle createFont(vg::Context* _ctx, const char* _name, const char* _filePath)
{
	uint32_t size;
	void* data = load(_filePath, &size);
	if (NULL == data) {
		return VG_INVALID_HANDLE;
	}

	return vg::createFont(_ctx, _name, (uint8_t*)data, size, 0);
}

vg::ImageHandle createImage(vg::Context* _ctx, const char* _filePath, int _imageFlags)
{
	uint32_t size;
	void* data = load(_filePath, &size);
	if (NULL == data) {
		return VG_INVALID_HANDLE;
	}

	bimg::ImageContainer* imageContainer = bimg::imageParse(
		entry::getAllocator()
		, data
		, size
		, bimg::TextureFormat::RGBA8
	);
	unload(data);

	if (NULL == imageContainer) {
		return VG_INVALID_HANDLE;
	}

	vg::ImageHandle texId = vg::createImage(
		_ctx
		, (uint16_t)imageContainer->m_width
		, (uint16_t)imageContainer->m_height
		, _imageFlags
		, (const uint8_t*)imageContainer->m_data
	);

	bimg::imageFree(imageContainer);

	return texId;
}

static void drawWindow(vg::Context* vgCtx, const char* title, float x, float y, float w, float h)
{
	float cornerRadius = 3.0f;
	vg::GradientHandle shadowPaint;
	vg::GradientHandle headerPaint;

//	vg::pushState(vgCtx);

	// Window
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x, y, w, h, cornerRadius);
	vg::fillPath(vgCtx, vg::color4ub(28, 30, 34, 192), vg::FillFlags::ConvexAA);

	// Drop shadow
	shadowPaint = vg::createBoxGradient(vgCtx, x, y + 2, w, h, cornerRadius * 2, 10, vg::color4ub(0, 0, 0, 128), vg::color4ub(0, 0, 0, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x - 10, y - 10, w + 20, h + 30);
	vg::roundedRect(vgCtx, x, y, w, h, cornerRadius);
	vg::fillPath(vgCtx, shadowPaint, vg::FillFlags::ConcaveEvenOddAA); // TODO(JD): Is this correct?

	// Header
	headerPaint = vg::createLinearGradient(vgCtx, x, y, x, y + 15, vg::color4ub(255, 255, 255, 8), vg::color4ub(0, 0, 0, 16));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 1, y + 1, w - 2, 30, cornerRadius - 1);
	vg::fillPath(vgCtx, headerPaint, vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::moveTo(vgCtx, x + 0.5f, y + 0.5f + 30);
	vg::lineTo(vgCtx, x + 0.5f + w - 1, y + 0.5f + 30);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 32), 1.0f, vg::StrokeFlags::ButtMiterAA);

	// NOTE: Font blurring isn't currently supported in vg-renderer.
//	nvgFontSize(vg, 18.0f);
//	nvgFontFace(vg, "sans-bold");
//	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
//	nvgFontBlur(vg, 2);
//	nvgFillColor(vg, nvgRGBA(0, 0, 0, 128));
//	nvgText(vg, x + w / 2, y + 16 + 1, title, NULL);
//	nvgFontBlur(vg, 0);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans-bold", 18.0f, vg::TextAlign::MiddleCenter, vg::color4ub(220, 220, 220, 160));
	vg::text(vgCtx, txtCfg, x + w / 2, y + 16, title, NULL);

//	vg::popState(vgCtx);
}

static void drawSearchBox(vg::Context* vgCtx, const char* text, float x, float y, float w, float h)
{
	vg::GradientHandle bg;
	char icon[8];
	float cornerRadius = h / 2 - 1;

	bg = vg::createBoxGradient(vgCtx, x, y + 1.5f, w, h, h / 2, 5, vg::color4ub(0, 0, 0, 16), vg::color4ub(0, 0, 0, 92));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x, y, w, h, cornerRadius);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "icons", h * 1.3f, vg::TextAlign::MiddleCenter, vg::color4ub(255, 255, 255, 64));
	vg::text(vgCtx, txtCfg, x + h * 0.55f, y + h * 0.55f, cpToUTF8(ICON_SEARCH, icon), NULL);

	vg::TextConfig txtCfg2 = vg::makeTextConfig(vgCtx, "sans", 20.0f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 32));
	vg::text(vgCtx, txtCfg2, x + h * 1.05f, y + h * 0.5f, text, NULL);

	vg::TextConfig txtCfg3 = vg::makeTextConfig(vgCtx, "icons", h * 1.3f, vg::TextAlign::MiddleCenter, vg::color4ub(255, 255, 255, 32));
	vg::text(vgCtx, txtCfg3, x + w - h * 0.55f, y + h * 0.55f, cpToUTF8(ICON_CIRCLED_CROSS, icon), NULL);
}

static void drawDropDown(vg::Context* vgCtx, const char* text, float x, float y, float w, float h)
{
	vg::GradientHandle bg;
	char icon[8];
	float cornerRadius = 4.0f;

	bg = vg::createLinearGradient(vgCtx, x, y, x, y + h, vg::color4ub(255, 255, 255, 16), vg::color4ub(0, 0, 0, 16));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 48), 1.0f, vg::StrokeFlags::ButtMiterAA);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans", 20.0f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 160));
	vg::text(vgCtx, txtCfg, x + h * 0.3f, y + h * 0.5f, text, NULL);

	vg::TextConfig txtCfg2 = vg::makeTextConfig(vgCtx, "icons", h * 1.3f, vg::TextAlign::MiddleCenter, vg::color4ub(255, 255, 255, 64));
	vg::text(vgCtx, txtCfg2, x + w - h * 0.5f, y + h * 0.5f, cpToUTF8(ICON_CHEVRON_RIGHT, icon), NULL);
}

static void drawLabel(vg::Context* vgCtx, const char* text, float x, float y, float w, float h)
{
	BX_UNUSED(w);
	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans", 18.0f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 128));
	vg::text(vgCtx, txtCfg, x, y + h * 0.5f, text, NULL);
}

static void drawEditBoxBase(vg::Context* vgCtx, float x, float y, float w, float h)
{
	vg::GradientHandle bg;
	// Edit
	bg = vg::createBoxGradient(vgCtx, x + 1, y + 1 + 1.5f, w - 2, h - 2, 3, 4, vg::color4ub(255, 255, 255, 32), vg::color4ub(32, 32, 32, 32));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 1, y + 1, w - 2, h - 2, 4 - 1);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 0.5f, y + 0.5f, w - 1, h - 1, 4 - 0.5f);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 48), 1.0f, vg::StrokeFlags::ButtMiterAA);
}

static void drawEditBox(vg::Context* vgCtx, const char* text, float x, float y, float w, float h)
{
	drawEditBoxBase(vgCtx, x, y, w, h);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans", 20.0f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 64));
	vg::text(vgCtx, txtCfg, x + h * 0.3f, y + h * 0.5f, text, NULL);
}

static void drawEditBoxNum(vg::Context* vgCtx, const char* text, const char* units, float x, float y, float w, float h)
{
	float uw;

	drawEditBoxBase(vgCtx, x, y, w, h);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans", 18.0f, vg::TextAlign::MiddleRight, vg::color4ub(255, 255, 255, 64));
	uw = vg::measureText(vgCtx, txtCfg, 0.0f, 0.0f, units, NULL, NULL);
	vg::text(vgCtx, txtCfg, x + w - h * 0.3f, y + h * 0.5f, units, NULL);

	vg::TextConfig txtCfg2 = vg::makeTextConfig(vgCtx, "sans", 20.0f, vg::TextAlign::MiddleRight, vg::color4ub(255, 255, 255, 128));
	vg::text(vgCtx, txtCfg2, x + w - uw - h * 0.5f, y + h * 0.5f, text, NULL);
}

static void drawCheckBox(vg::Context* vgCtx, const char* text, float x, float y, float w, float h)
{
	vg::GradientHandle bg;
	char icon[8];
	BX_UNUSED(w);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans", 18.0f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 160));
	vg::text(vgCtx, txtCfg, x + 28, y + h * 0.5f, text, NULL);

	bg = vg::createBoxGradient(vgCtx, x + 1, y + (int)(h * 0.5f) - 9 + 1, 18, 18, 3, 3, vg::color4ub(0, 0, 0, 32), vg::color4ub(0, 0, 0, 92));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 1, y + (int)(h * 0.5f) - 9, 18, 18, 3);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	vg::TextConfig txtCfg2 = vg::makeTextConfig(vgCtx, "icons", 40.0f, vg::TextAlign::MiddleCenter, vg::color4ub(255, 255, 255, 128));
	vg::text(vgCtx, txtCfg2, x + 9 + 2, y + h * 0.5f, cpToUTF8(ICON_CHECK, icon), NULL);
}

static void drawButton(vg::Context* vgCtx, int preicon, const char* text, float x, float y, float w, float h, vg::Color col)
{
	vg::GradientHandle bg;
	char icon[8];
	float cornerRadius = 4.0f;
	float tw = 0, iw = 0;

	bg = vg::createLinearGradient(vgCtx, x, y, x, y + h, vg::color4ub(255, 255, 255, isBlack(col) ? 16 : 32), vg::color4ub(0, 0, 0, isBlack(col) ? 16 : 32));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);
	if (!isBlack(col)) {
		vg::fillPath(vgCtx, col, vg::FillFlags::Convex);
	}
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 48), 1.0f, vg::StrokeFlags::ButtMiterAA);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, "sans-bold", 20.0f, vg::TextAlign::MiddleLeft, vg::color4ub(0, 0, 0, 160));
	tw = vg::measureText(vgCtx, txtCfg, 0, 0, text, NULL, NULL);

	if (preicon != 0) {
		vg::TextConfig txtCfg2 = vg::makeTextConfig(vgCtx, "icons", h * 1.3f, vg::TextAlign::MiddleLeft, vg::color4ub(255, 255, 255, 96));
		iw = vg::measureText(vgCtx, txtCfg2, 0, 0, cpToUTF8(preicon, icon), NULL, NULL);
		iw += h * 0.15f;

		vg::text(vgCtx, txtCfg2, x + w * 0.5f - tw * 0.5f - iw * 0.75f, y + h * 0.5f, cpToUTF8(preicon, icon), NULL);
	}

	vg::text(vgCtx, txtCfg, x + w * 0.5f - tw * 0.5f + iw * 0.25f, y + h * 0.5f - 1, text, NULL);

	txtCfg.m_Color = vg::color4ub(255, 255, 255, 160);
	vg::text(vgCtx, txtCfg, x + w * 0.5f - tw * 0.5f + iw * 0.25f, y + h * 0.5f, text, NULL);
}

static void drawSlider(vg::Context* vgCtx, float pos, float x, float y, float w, float h)
{
	vg::GradientHandle bg, knob;
	float cy = y + (int)(h * 0.5f);
	float kr = (float)((int)(h * 0.25f));

//	vg::pushState(vgCtx);

	// Slot
	bg = vg::createBoxGradient(vgCtx, x, cy - 2 + 1, w, 4, 2, 2, vg::color4ub(0, 0, 0, 32), vg::color4ub(0, 0, 0, 128));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x, cy - 2, w, 4, 2);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	// Knob Shadow
	bg = vg::createRadialGradient(vgCtx, x + (int)(pos * w), cy + 1, kr - 3, kr + 3, vg::color4ub(0, 0, 0, 64), vg::color4ub(0, 0, 0, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x + (int)(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
	vg::circle(vgCtx, x + (int)(pos * w), cy, kr);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConcaveEvenOddAA);

	// Knob
	knob = vg::createLinearGradient(vgCtx, x, cy - kr, x, cy + kr, vg::color4ub(255, 255, 255, 16), vg::color4ub(0, 0, 0, 16));
	vg::beginPath(vgCtx);
	vg::circle(vgCtx, x + (int)(pos * w), cy, kr - 1);
	vg::fillPath(vgCtx, vg::color4ub(40, 43, 48, 255), vg::FillFlags::Convex);
	vg::fillPath(vgCtx, knob, vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::circle(vgCtx, x + (int)(pos * w), cy, kr - 0.5f);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 92), 1.0f, vg::StrokeFlags::ButtMiterAA);

//	vg::popState(vgCtx);
}

static void drawEyes(vg::Context* vgCtx, float x, float y, float w, float h, float mx, float my, float t)
{
	vg::GradientHandle gloss, bg;
	float ex = w * 0.23f;
	float ey = h * 0.5f;
	float lx = x + ex;
	float ly = y + ey;
	float rx = x + w - ex;
	float ry = y + ey;
	float dx, dy, d;
	float br = (ex < ey ? ex : ey) * 0.5f;
	float blink = 1 - powf(sinf(t * 0.5f), 200) * 0.8f;

	bg = vg::createLinearGradient(vgCtx, x, y + h * 0.5f, x + w * 0.1f, y + h, vg::color4ub(0, 0, 0, 32), vg::color4ub(0, 0, 0, 16));
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, lx + 3.0f, ly + 16.0f, ex, ey);
	vg::ellipse(vgCtx, rx + 3.0f, ry + 16.0f, ex, ey);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	bg = vg::createLinearGradient(vgCtx, x, y + h * 0.25f, x + w * 0.1f, y + h, vg::color4ub(220, 220, 220, 255), vg::color4ub(128, 128, 128, 255));
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, lx, ly, ex, ey);
	vg::ellipse(vgCtx, rx, ry, ex, ey);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConvexAA);

	dx = (mx - rx) / (ex * 10);
	dy = (my - ry) / (ey * 10);
	d = sqrtf(dx * dx + dy * dy);
	if (d > 1.0f) {
		dx /= d; dy /= d;
	}
	dx *= ex * 0.4f;
	dy *= ey * 0.5f;
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, lx + dx, ly + dy + ey * 0.25f * (1 - blink), br, br * blink);
	vg::fillPath(vgCtx, vg::color4ub(32, 32, 32, 255), vg::FillFlags::ConvexAA);

	dx = (mx - rx) / (ex * 10);
	dy = (my - ry) / (ey * 10);
	d = sqrtf(dx * dx + dy * dy);
	if (d > 1.0f) {
		dx /= d; dy /= d;
	}
	dx *= ex * 0.4f;
	dy *= ey * 0.5f;
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, rx + dx, ry + dy + ey * 0.25f * (1 - blink), br, br * blink);
	vg::fillPath(vgCtx, vg::color4ub(32, 32, 32, 255), vg::FillFlags::ConvexAA);

	gloss = vg::createRadialGradient(vgCtx, lx - ex * 0.25f, ly - ey * 0.5f, ex * 0.1f, ex * 0.75f, vg::color4ub(255, 255, 255, 128), vg::color4ub(255, 255, 255, 0));
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, lx, ly, ex, ey);
	vg::fillPath(vgCtx, gloss, vg::FillFlags::ConvexAA);

	gloss = vg::createRadialGradient(vgCtx, rx - ex * 0.25f, ry - ey * 0.5f, ex * 0.1f, ex * 0.75f, vg::color4ub(255, 255, 255, 128), vg::color4ub(255, 255, 255, 0));
	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, rx, ry, ex, ey);
	vg::fillPath(vgCtx, gloss, vg::FillFlags::ConvexAA);
}

static void drawGraph(vg::Context* vgCtx, float x, float y, float w, float h, float t)
{
	float samples[6];
	float sx[6], sy[6];
	float dx = w / 5.0f;
	int i;

	samples[0] = (1 + sinf(t * 1.2345f + cosf(t * 0.33457f) * 0.44f)) * 0.5f;
	samples[1] = (1 + sinf(t * 0.68363f + cosf(t * 1.3f) * 1.55f)) * 0.5f;
	samples[2] = (1 + sinf(t * 1.1642f + cosf(t * 0.33457f) * 1.24f)) * 0.5f;
	samples[3] = (1 + sinf(t * 0.56345f + cosf(t * 1.63f) * 0.14f)) * 0.5f;
	samples[4] = (1 + sinf(t * 1.6245f + cosf(t * 0.254f) * 0.3f)) * 0.5f;
	samples[5] = (1 + sinf(t * 0.345f + cosf(t * 0.03f) * 0.6f)) * 0.5f;

	for (i = 0; i < 6; i++) {
		sx[i] = x + i * dx;
		sy[i] = y + h * samples[i] * 0.8f;
	}

	// Graph background
	vg::GradientHandle bg = vg::createLinearGradient(vgCtx, x, y, x, y + h, vg::color4ub(0, 160, 192, 0), vg::color4ub(0, 160, 192, 64));
	vg::beginPath(vgCtx);
	vg::moveTo(vgCtx, sx[0], sy[0]);
	for (i = 1; i < 6; i++) {
		vg::cubicTo(vgCtx, sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f, sy[i], sx[i], sy[i]);
	}
	vg::lineTo(vgCtx, x + w, y + h);
	vg::lineTo(vgCtx, x, y + h);
	vg::fillPath(vgCtx, bg, vg::FillFlags::ConcaveNonZeroAA);

	// Graph line
	vg::beginPath(vgCtx);
	vg::moveTo(vgCtx, sx[0], sy[0] + 2);
	for (i = 1; i < 6; i++) {
		vg::cubicTo(vgCtx, sx[i - 1] + dx * 0.5f, sy[i - 1] + 2, sx[i] - dx * 0.5f, sy[i] + 2, sx[i], sy[i] + 2);
	}
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 32), 3.0f, vg::StrokeFlags::ButtMiterAA);

	vg::beginPath(vgCtx);
	vg::moveTo(vgCtx, sx[0], sy[0]);
	for (i = 1; i < 6; i++) {
		vg::cubicTo(vgCtx, sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f, sy[i], sx[i], sy[i]);
	}
	vg::strokePath(vgCtx, vg::color4ub(0, 160, 192, 255), 3.0f, vg::StrokeFlags::ButtMiterAA);

	// Graph sample pos
	for (i = 0; i < 6; i++) {
		bg = vg::createRadialGradient(vgCtx, sx[i], sy[i] + 2, 3.0f, 8.0f, vg::color4ub(0, 0, 0, 32), vg::color4ub(0, 0, 0, 0));
		vg::beginPath(vgCtx);
		vg::rect(vgCtx, sx[i] - 10, sy[i] - 10 + 2, 20, 20);
		vg::fillPath(vgCtx, bg, vg::FillFlags::ConcaveNonZeroAA);
	}

	vg::beginPath(vgCtx);
	for (i = 0; i < 6; i++) {
		vg::circle(vgCtx, sx[i], sy[i], 4.0f);
	}
	vg::fillPath(vgCtx, vg::color4ub(0, 160, 192, 255), vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	for (i = 0; i < 6; i++) {
		vg::circle(vgCtx, sx[i], sy[i], 2.0f);
	}
	vg::fillPath(vgCtx, vg::color4ub(220, 220, 220, 255), vg::FillFlags::ConvexAA);
}

static void drawSpinner(vg::Context* vgCtx, float cx, float cy, float r, float t)
{
	float a0 = 0.0f + t * 6;
	float a1 = bx::kPi + t * 6;
	float r0 = r;
	float r1 = r * 0.75f;
	float ax, ay, bx, by;
	vg::GradientHandle paint;

	vg::pushState(vgCtx);

	vg::beginPath(vgCtx);
	vg::arc(vgCtx, cx, cy, r0, a0, a1, vg::Winding::CW);
	vg::arc(vgCtx, cx, cy, r1, a1, a0, vg::Winding::CCW);
	vg::closePath(vgCtx);
	ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
	ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
	bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
	by = cy + sinf(a1) * (r0 + r1) * 0.5f;
	paint = vg::createLinearGradient(vgCtx, ax, ay, bx, by, vg::color4ub(0, 0, 0, 0), vg::color4ub(0, 0, 0, 128));
	vg::fillPath(vgCtx, paint, vg::FillFlags::ConcaveAA);

	vg::popState(vgCtx);
}

static void drawThumbnails(vg::Context* vgCtx, float x, float y, float w, float h, const vg::ImageHandle* images, int nimages, float t)
{
	float cornerRadius = 3.0f;
	vg::GradientHandle shadowPaint, fadePaint;
	vg::ImagePatternHandle imgPaint;
	float ix, iy, iw, ih;
	float thumb = 60.0f;
	float arry = 30.5f;
	uint16_t imgw, imgh;
	float stackh = (nimages / 2) * (thumb + 10) + 10;
	int i;
	float u = (1 + cosf(t * 0.5f)) * 0.5f;
	float u2 = (1 - cosf(t * 0.2f)) * 0.5f;
	float scrollh, dv;

	vg::pushState(vgCtx);

	// Drop shadow
	shadowPaint = vg::createBoxGradient(vgCtx, x, y + 4, w, h, cornerRadius * 2, 20, vg::color4ub(0, 0, 0, 128), vg::color4ub(0, 0, 0, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x - 10, y - 10, w + 20, h + 30);
	vg::roundedRect(vgCtx, x, y, w, h, cornerRadius);
	vg::fillPath(vgCtx, shadowPaint, vg::FillFlags::ConcaveEvenOddAA);

	// Window
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x, y, w, h, cornerRadius);
	vg::moveTo(vgCtx, x - 10, y + arry);
	vg::lineTo(vgCtx, x + 1, y + arry - 11);
	vg::lineTo(vgCtx, x + 1, y + arry + 11);
	vg::fillPath(vgCtx, vg::color4ub(200, 200, 200, 255), vg::FillFlags::ConvexAA);

	vg::pushState(vgCtx);
	vg::setScissor(vgCtx, x, y, w, h);
	vg::transformTranslate(vgCtx, 0, -(stackh - h) * u);

	dv = 1.0f / (float)(nimages - 1);

	for (i = 0; i < nimages; i++) {
		float tx, ty, v, a;
		tx = x + 10;
		ty = y + 10;
		tx += (i % 2) * (thumb + 10);
		ty += (i / 2) * (thumb + 10);
		vg::getImageSize(vgCtx, images[i], &imgw, &imgh);
		if (imgw < imgh) {
			iw = thumb;
			ih = iw * (float)imgh / (float)imgw;
			ix = 0;
			iy = -(ih - thumb) * 0.5f;
		} else {
			ih = thumb;
			iw = ih * (float)imgw / (float)imgh;
			ix = -(iw - thumb) * 0.5f;
			iy = 0;
		}

		v = i * dv;
		a = bx::clamp((u2 - v) / dv, 0.0f, 1.0f);

		if (a < 1.0f) {
			drawSpinner(vgCtx, tx + thumb / 2, ty + thumb / 2, thumb * 0.25f, t);
		}

		imgPaint = vg::createImagePattern(vgCtx, tx + ix, ty + iy, iw, ih, 0.0f / 180.0f * bx::kPi, images[i]);
		vg::beginPath(vgCtx);
		vg::roundedRect(vgCtx, tx, ty, thumb, thumb, 5);
		vg::fillPath(vgCtx, imgPaint, vg::color4f(1.0f, 1.0f, 1.0f, a), vg::FillFlags::ConvexAA);

		shadowPaint = vg::createBoxGradient(vgCtx, tx - 1, ty, thumb + 2, thumb + 2, 5, 3, vg::color4ub(0, 0, 0, 128), vg::color4ub(0, 0, 0, 0));
		vg::beginPath(vgCtx);
		vg::rect(vgCtx, tx - 5, ty - 5, thumb + 10, thumb + 10);
		vg::roundedRect(vgCtx, tx, ty, thumb, thumb, 6);
		vg::fillPath(vgCtx, shadowPaint, vg::FillFlags::ConcaveEvenOddAA);

		vg::beginPath(vgCtx);
		vg::roundedRect(vgCtx, tx + 0.5f, ty + 0.5f, thumb - 1, thumb - 1, 4 - 0.5f);
		vg::strokePath(vgCtx, vg::color4ub(255, 255, 255, 192), 1.0f, vg::StrokeFlags::ButtMiterAA);
	}
	vg::popState(vgCtx);

	// Hide fades
	fadePaint = vg::createLinearGradient(vgCtx, x, y, x, y + 6, vg::color4ub(200, 200, 200, 255), vg::color4ub(200, 200, 200, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x + 4, y, w - 8, 6);
	vg::fillPath(vgCtx, fadePaint, vg::FillFlags::ConvexAA);

	fadePaint = vg::createLinearGradient(vgCtx, x, y + h, x, y + h - 6, vg::color4ub(200, 200, 200, 255), vg::color4ub(200, 200, 200, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x + 4, y + h - 6, w - 8, 6);
	vg::fillPath(vgCtx, fadePaint, vg::FillFlags::ConvexAA);

	// Scroll bar
	shadowPaint = vg::createBoxGradient(vgCtx, x + w - 12 + 1, y + 4 + 1, 8, h - 8, 3, 4, vg::color4ub(0, 0, 0, 32), vg::color4ub(0, 0, 0, 92));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + w - 12, y + 4, 8, h - 8, 3);
	vg::fillPath(vgCtx, shadowPaint, vg::FillFlags::ConvexAA);

	scrollh = (h / stackh) * (h - 8);
	shadowPaint = vg::createBoxGradient(vgCtx, x + w - 12 - 1, y + 4 + (h - 8 - scrollh) * u - 1, 8, scrollh, 3, 4, vg::color4ub(220, 220, 220, 255), vg::color4ub(128, 128, 128, 255));
	vg::beginPath(vgCtx);
	vg::roundedRect(vgCtx, x + w - 12 + 1, y + 4 + 1 + (h - 8 - scrollh) * u, 8 - 2, scrollh - 2, 2);
	vg::fillPath(vgCtx, shadowPaint, vg::FillFlags::ConvexAA);

	vg::popState(vgCtx);
}

static void drawColorwheel(vg::Context* vgCtx, float x, float y, float w, float h, float t)
{
	int i;
	float r0, r1, ax, ay, bx, by, cx, cy, aeps, r;
	float hue = sinf(t * 0.12f);
	vg::GradientHandle paint;

	vg::pushState(vgCtx);

	cx = x + w * 0.5f;
	cy = y + h * 0.5f;
	r1 = (w < h ? w : h) * 0.5f - 5.0f;
	r0 = r1 - 20.0f;
	aeps = 0.5f / r1;	// half a pixel arc length in radians (2pi cancels out).

	for (i = 0; i < 6; i++) {
		float a0 = (float)i / 6.0f * bx::kPi * 2.0f - aeps;
		float a1 = (float)(i + 1.0f) / 6.0f * bx::kPi * 2.0f + aeps;
		vg::beginPath(vgCtx);
		vg::arc(vgCtx, cx, cy, r0, a0, a1, vg::Winding::CW);
		vg::arc(vgCtx, cx, cy, r1, a1, a0, vg::Winding::CCW);
		vg::closePath(vgCtx);

		ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
		ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
		bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
		by = cy + sinf(a1) * (r0 + r1) * 0.5f;
		paint = vg::createLinearGradient(vgCtx, ax, ay, bx, by, vg::colorHSB(a0 / (bx::kPi * 2), 1.0f, 1.0f), vg::colorHSB(a1 / (bx::kPi * 2), 1.0f, 1.0f));
		vg::fillPath(vgCtx, paint, vg::FillFlags::ConcaveAA);
	}

	vg::beginPath(vgCtx);
	vg::circle(vgCtx, cx, cy, r0 - 0.5f);
	vg::circle(vgCtx, cx, cy, r1 + 0.5f);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 64), 1.0f, vg::StrokeFlags::ButtMiterAA);

	// Selector
	vg::pushState(vgCtx);
	vg::transformTranslate(vgCtx, cx, cy);
	vg::transformRotate(vgCtx, hue * bx::kPi * 2);

	// Marker on
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, r0 - 1, -3, r1 - r0 + 2, 6);
	vg::strokePath(vgCtx, vg::color4ub(255, 255, 255, 192), 2.0f, vg::StrokeFlags::ButtMiterAA);

	paint = vg::createBoxGradient(vgCtx, r0 - 3, -5, r1 - r0 + 6, 10, 2, 4, vg::color4ub(0, 0, 0, 128), vg::color4ub(0, 0, 0, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, r0 - 2 - 10, -4 - 10, r1 - r0 + 4 + 20, 8 + 20);
	vg::rect(vgCtx, r0 - 2, -4, r1 - r0 + 4, 8);
	vg::fillPath(vgCtx, paint, vg::FillFlags::ConcaveEvenOddAA);

	// Center triangle
	r = r0 - 6;
	ax = cosf(120.0f / 180.0f * bx::kPi) * r;
	ay = sinf(120.0f / 180.0f * bx::kPi) * r;
	bx = cosf(-120.0f / 180.0f * bx::kPi) * r;
	by = sinf(-120.0f / 180.0f * bx::kPi) * r;
	vg::beginPath(vgCtx);
	vg::moveTo(vgCtx, r, 0);
	vg::lineTo(vgCtx, ax, ay);
	vg::lineTo(vgCtx, bx, by);
	vg::closePath(vgCtx);
	paint = vg::createLinearGradient(vgCtx, r, 0, ax, ay, vg::colorHSB(hue, 1.0f, 1.0f), vg::color4ub(255, 255, 255, 255));
	vg::fillPath(vgCtx, paint, vg::FillFlags::ConvexAA);
	paint = vg::createLinearGradient(vgCtx, (r + ax) * 0.5f, (0 + ay) * 0.5f, bx, by, vg::color4ub(0, 0, 0, 0), vg::color4ub(0, 0, 0, 255));
	vg::fillPath(vgCtx, paint, vg::FillFlags::ConvexAA);
	vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 64), 2.0f, vg::StrokeFlags::ButtMiterAA);

	// Select circle on triangle
	ax = cosf(120.0f / 180.0f * bx::kPi) * r * 0.3f;
	ay = sinf(120.0f / 180.0f * bx::kPi) * r * 0.4f;
	vg::beginPath(vgCtx);
	vg::circle(vgCtx, ax, ay, 5);
	vg::strokePath(vgCtx, vg::color4ub(255, 255, 255, 192), 2.0f, vg::StrokeFlags::ButtMiterAA);

	paint = vg::createRadialGradient(vgCtx, ax, ay, 7, 9, vg::color4ub(0, 0, 0, 64), vg::color4ub(0, 0, 0, 0));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, ax - 20, ay - 20, 40, 40);
	vg::circle(vgCtx, ax, ay, 7);
	vg::fillPath(vgCtx, paint, vg::FillFlags::ConcaveEvenOddAA);

	vg::popState(vgCtx);

	vg::popState(vgCtx);
}

static void drawLines(vg::Context* vgCtx, float x, float y, float w, float h, float t)
{
	int i, j;
	float pad = 5.0f, s = w / 9.0f - pad * 2;
	float pts[4 * 2], fx, fy;
	vg::LineJoin::Enum joins[3] = { vg::LineJoin::Miter, vg::LineJoin::Round, vg::LineJoin::Bevel };
	vg::LineCap::Enum caps[3] = { vg::LineCap::Butt, vg::LineCap::Round, vg::LineCap::Square };
	BX_UNUSED(h);

	// NOTE: This isn't needed in the case of vg-renderer because stroke parameters
	// are not state variables.
//	vg::pushState(vgCtx);

	pts[0] = -s * 0.25f + cosf(t * 0.3f) * s * 0.5f;
	pts[1] = sinf(t * 0.3f) * s * 0.5f;
	pts[2] = -s * 0.25f;
	pts[3] = 0;
	pts[4] = s * 0.25f;
	pts[5] = 0;
	pts[6] = s * 0.25f + cosf(-t * 0.3f) * s * 0.5f;
	pts[7] = sinf(-t * 0.3f) * s * 0.5f;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			fx = x + s * 0.5f + (i * 3 + j) / 9.0f * w + pad;
			fy = y - s * 0.5f + pad;

			vg::beginPath(vgCtx);
			vg::moveTo(vgCtx, fx + pts[0], fy + pts[1]);
			vg::lineTo(vgCtx, fx + pts[2], fy + pts[3]);
			vg::lineTo(vgCtx, fx + pts[4], fy + pts[5]);
			vg::lineTo(vgCtx, fx + pts[6], fy + pts[7]);
			vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 160), s * 0.3f, VG_STROKE_FLAGS(caps[i], joins[i], 1));

			vg::beginPath(vgCtx);
			vg::moveTo(vgCtx, fx + pts[0], fy + pts[1]);
			vg::lineTo(vgCtx, fx + pts[2], fy + pts[3]);
			vg::lineTo(vgCtx, fx + pts[4], fy + pts[5]);
			vg::lineTo(vgCtx, fx + pts[6], fy + pts[7]);
			vg::strokePath(vgCtx, vg::color4ub(0, 192, 255, 255), 1.0f, vg::StrokeFlags::ButtBevelAA);
		}
	}

//	vg::popState(vgCtx);
}

static void drawWidths(vg::Context* vgCtx, float x, float y, float width)
{
	int i;

	// NOTE: This isn't needed in the case of vg-renderer because stroke parameters
	// are not state variables.
//	vg::pushState(vgCtx);

	for (i = 0; i < 20; i++) {
		float w = (i + 0.5f) * 0.1f;
		vg::beginPath(vgCtx);
		vg::moveTo(vgCtx, x, y);
		vg::lineTo(vgCtx, x + width, y + width * 0.3f);
		vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 255), w, vg::StrokeFlags::ButtMiterAA);
		y += 10;
	}

//	vg::popState(vgCtx);
}

static void drawCaps(vg::Context* vgCtx, float x, float y, float width)
{
	int i;
	vg::LineCap::Enum caps[3] = { vg::LineCap::Butt, vg::LineCap::Round, vg::LineCap::Square };
	float lineWidth = 8.0f;

	// NOTE: This isn't needed in the case of vg-renderer because stroke parameters
	// are not state variables.
//	vg::pushState(vgCtx);

	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x - lineWidth / 2, y, width + lineWidth, 40);
	vg::fillPath(vgCtx, vg::color4ub(255, 255, 255, 32), vg::FillFlags::ConvexAA);

	vg::beginPath(vgCtx);
	vg::rect(vgCtx, x, y, width, 40);
	vg::fillPath(vgCtx, vg::color4ub(255, 255, 255, 32), vg::FillFlags::ConvexAA);

	for (i = 0; i < 3; i++) {
		vg::beginPath(vgCtx);
		vg::moveTo(vgCtx, x, y + i * 10 + 5);
		vg::lineTo(vgCtx, x + width, y + i * 10 + 5);
		vg::strokePath(vgCtx, vg::color4ub(0, 0, 0, 255), lineWidth, VG_STROKE_FLAGS(caps[i], vg::LineJoin::Miter, 1));
	}

//	vg::popState(vgCtx);
}

// TODO(JD): Doesn't render correctly compared to NanoVG.
static void drawScissor(vg::Context* vgCtx, float x, float y, float t)
{
	vg::pushState(vgCtx);

	// Draw first rect and set scissor to it's area.
	vg::transformTranslate(vgCtx, x, y);
	vg::transformRotate(vgCtx, bx::toRad(5.0f));
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, -20, -20, 60, 40);
	vg::fillPath(vgCtx, vg::color4ub(255, 0, 0, 255), vg::FillFlags::ConvexAA);

	vg::setScissor(vgCtx, -20, -20, 60, 40);

	// Draw second rectangle with offset and rotation.
	vg::transformTranslate(vgCtx, 40, 0);
	vg::transformRotate(vgCtx, t);

	// Draw the intended second rectangle without any scissoring.
	vg::pushState(vgCtx);
	vg::resetScissor(vgCtx);
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, -20, -10, 60, 30);
	vg::fillPath(vgCtx, vg::color4ub(255, 128, 0, 64), vg::FillFlags::ConvexAA);
	vg::popState(vgCtx);

	// Draw second rectangle with combined scissoring.
	vg::intersectScissor(vgCtx, -20, -10, 60, 30);
	vg::beginPath(vgCtx);
	vg::rect(vgCtx, -20, -10, 60, 30);
	vg::fillPath(vgCtx, vg::color4ub(255, 128, 0, 255), vg::FillFlags::ConvexAA);

	vg::popState(vgCtx);
}

void drawBlendish(vg::Context* _vg, float _x, float _y, float _w, float _h, float _t)
{
	float x = _x;
	float y = _y;

	bndBackground(_vg, _x - 10.0f, _y - 10.0f, _w, _h);

	bndToolButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, BND_ICONID(6, 3), "Default");
	y += 25.0f;
	bndToolButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, BND_ICONID(6, 3), "Hovered");
	y += 25.0f;
	bndToolButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, BND_ICONID(6, 3), "Active");

	y += 40.0f;
	bndRadioButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, -1, "Default");
	y += 25.0f;
	bndRadioButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, -1, "Hovered");
	y += 25.0f;
	bndRadioButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, -1, "Active");

	y += 25.0f;
	bndLabel(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, -1, "Label:");
	y += float(BND_WIDGET_HEIGHT);
	bndChoiceButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, -1, "Default");
	y += 25.0f;
	bndChoiceButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, -1, "Hovered");
	y += 25.0f;
	bndChoiceButton(_vg, x, y, 80.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, -1, "Active");

	y += 25.0f;
	float ry = y;
	float rx = x;

	y = _y;
	x += 130.0f;
	bndOptionButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_DEFAULT, "Default");
	y += 25.0f;
	bndOptionButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_HOVER, "Hovered");
	y += 25.0f;
	bndOptionButton(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_ACTIVE, "Active");

	y += 40.0f;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_DOWN, BND_DEFAULT, "Top", "100");
	y += float(BND_WIDGET_HEIGHT) - 2.0f;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, "Center", "100");
	y += float(BND_WIDGET_HEIGHT) - 2.0f;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_TOP, BND_DEFAULT, "Bottom", "100");

	float mx = x - 30.0f;
	float my = y - 12.0f;
	float mw = 120.0f;
	bndMenuBackground(_vg, mx, my, mw, 120.0f, BND_CORNER_TOP);
	bndMenuLabel(_vg, mx, my, mw, BND_WIDGET_HEIGHT, -1, "Menu Title");
	my += float(BND_WIDGET_HEIGHT) - 2.0f;
	bndMenuItem(_vg, mx, my, mw, BND_WIDGET_HEIGHT, BND_DEFAULT, BND_ICONID(17, 3), "Default");
	my += float(BND_WIDGET_HEIGHT) - 2.0f;
	bndMenuItem(_vg, mx, my, mw, BND_WIDGET_HEIGHT, BND_HOVER, BND_ICONID(18, 3), "Hovered");
	my += float(BND_WIDGET_HEIGHT) - 2.0f;
	bndMenuItem(_vg, mx, my, mw, BND_WIDGET_HEIGHT, BND_ACTIVE, BND_ICONID(19, 3), "Active");

	y = _y;
	x += 130.0f;
	float ox = x;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, "Default", "100");
	y += 25.0f;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, "Hovered", "100");
	y += 25.0f;
	bndNumberField(_vg, x, y, 120.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, "Active", "100");

	y += 40.0f;
	bndRadioButton(_vg, x, y, 60.0f, BND_WIDGET_HEIGHT, BND_CORNER_RIGHT, BND_DEFAULT, -1, "One");
	x += 60.0f - 1.0f;
	bndRadioButton(_vg, x, y, 60.0f, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, -1, "Two");
	x += 60.0f - 1.0f;
	bndRadioButton(_vg, x, y, 60.0f, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, -1, "Three");
	x += 60.0f - 1.0f;
	bndRadioButton(_vg, x, y, 60.0f, BND_WIDGET_HEIGHT, BND_CORNER_LEFT, BND_ACTIVE, -1, "Butts");

	x = ox;
	y += 40.0f;
	float progress_value = fmodf(_t / 10.0f, 1.0f);
	char progress_label[32];
	bx::snprintf(progress_label, BX_COUNTOF(progress_label), "%d%%", int(progress_value * 100 + 0.5f));
	bndSlider(_vg, x, y, 240, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, progress_value, "Default", progress_label);
	y += 25.0f;
	bndSlider(_vg, x, y, 240, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, progress_value, "Hovered", progress_label);
	y += 25.0f;
	bndSlider(_vg, x, y, 240, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, progress_value, "Active", progress_label);

	float rw = x + 240.0f - rx;
	float s_offset = sinf(_t / 2.0f) * 0.5f + 0.5f;
	float s_size = cosf(_t / 3.11f) * 0.5f + 0.5f;

	bndScrollBar(_vg, rx, ry, rw, BND_SCROLLBAR_HEIGHT, BND_DEFAULT, s_offset, s_size);
	ry += 20.0f;
	bndScrollBar(_vg, rx, ry, rw, BND_SCROLLBAR_HEIGHT, BND_HOVER, s_offset, s_size);
	ry += 20.0f;
	bndScrollBar(_vg, rx, ry, rw, BND_SCROLLBAR_HEIGHT, BND_ACTIVE, s_offset, s_size);

	const char edit_text[] = "The quick brown fox";
	int textlen = int(strlen(edit_text) + 1);
	int t = int(_t * 2);
	int idx1 = (t / textlen) % textlen;
	int idx2 = idx1 + (t % (textlen - idx1));

	ry += 25.0f;
	bndTextField(_vg, rx, ry, 240.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_DEFAULT, -1, edit_text, idx1, idx2);
	ry += 25.0f;
	bndTextField(_vg, rx, ry, 240.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_HOVER, -1, edit_text, idx1, idx2);
	ry += 25.0f;
	bndTextField(_vg, rx, ry, 240.0f, BND_WIDGET_HEIGHT, BND_CORNER_NONE, BND_ACTIVE, -1, edit_text, idx1, idx2);

	rx += rw + 20.0f;
	ry = _y;
	bndScrollBar(_vg, rx, ry, BND_SCROLLBAR_WIDTH, 240.0f, BND_DEFAULT, s_offset, s_size);
	rx += 20.0f;
	bndScrollBar(_vg, rx, ry, BND_SCROLLBAR_WIDTH, 240.0f, BND_HOVER, s_offset, s_size);
	rx += 20.0f;
	bndScrollBar(_vg, rx, ry, BND_SCROLLBAR_WIDTH, 240.0f, BND_ACTIVE, s_offset, s_size);

	x = ox;
	y += 40.0f;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_RIGHT, BND_DEFAULT, BND_ICONID(0, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(1, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(2, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(3, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(4, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndToolButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_LEFT, BND_DEFAULT, BND_ICONID(5, 10), NULL);
	x += BND_TOOL_WIDTH - 1;
	x += 5.0f;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_RIGHT, BND_DEFAULT, BND_ICONID(0, 11), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(1, 11), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(2, 11), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_DEFAULT, BND_ICONID(3, 11), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_ALL, BND_ACTIVE, BND_ICONID(4, 11), NULL);
	x += BND_TOOL_WIDTH - 1;
	bndRadioButton(_vg, x, y, BND_TOOL_WIDTH, BND_WIDGET_HEIGHT, BND_CORNER_LEFT, BND_DEFAULT, BND_ICONID(5, 11), NULL);
}

static void renderNanoVGDemo(vg::Context* vgCtx, float mx, float my, float width, float height, float t, int blowup, NanoVGDemoData* data)
{
	BX_UNUSED(vgCtx, mx, my, width, height, t, blowup, data);
	float x, y, popx, popy;

	drawEyes(vgCtx, width - 800, height - 240, 150, 100, mx, my, t);
//	drawParagraph(vg, width - 550, 35, 150, 100, mx, my);
	drawGraph(vgCtx, 0, height / 2, width, height / 2, t);

	drawColorwheel(vgCtx, width - 350, 35, 250.0f, 250.0f, t);

	// Line joints
	drawLines(vgCtx, 50, height - 50, 600, 35, t);

	// Line caps
	drawWidths(vgCtx, width - 50, 35, 30);

	// Line caps
	drawCaps(vgCtx, width - 50, 260, 30);

	drawScissor(vgCtx, 40, height - 150, t);

	vg::pushState(vgCtx);
	if (blowup) {
		vg::transformRotate(vgCtx, sinf(t * 0.3f) * 5.0f / 180.0f * bx::kPi);
		vg::transformScale(vgCtx, 2.0f, 2.0f);
	}

	// Widgets.
	x = width - 520; y = height - 420;
	drawWindow(vgCtx, "Widgets `n Stuff", x, y, 300, 400);
	x += 10;
	y += 45;
	drawSearchBox(vgCtx, "Search", x, y, 280, 25);
	y += 40;
	drawDropDown(vgCtx, "Effects", x, y, 280, 28);
	popx = x + 300;
	popy = y + 14;
	y += 45;

	// Form
	drawLabel(vgCtx, "Login", x, y, 280, 20);
	y += 25;
	drawEditBox(vgCtx, "Email", x, y, 280, 28);
	y += 35;
	drawEditBox(vgCtx, "Password", x, y, 280, 28);
	y += 38;
	drawCheckBox(vgCtx, "Remember me", x, y, 140, 28);
	drawButton(vgCtx, ICON_LOGIN, "Sign in", x + 138, y, 140, 28, vg::color4ub(0, 96, 128, 255));
	y += 45;

	// Slider
	drawLabel(vgCtx, "Diameter", x, y, 280, 20);
	y += 25;
	drawEditBoxNum(vgCtx, "123.00", "px", x + 180, y, 100, 28);
	drawSlider(vgCtx, 0.4f, x, y, 170, 28);
	y += 55;

	drawButton(vgCtx, ICON_TRASH, "Delete", x, y, 160, 28, vg::color4ub(128, 16, 8, 255));
	drawButton(vgCtx, 0, "Cancel", x + 170, y, 110, 28, vg::color4ub(0, 0, 0, 0));

	// Thumbnails box
	drawThumbnails(vgCtx, popx, popy - 30, 160, 300, data->images, 12, t);

	// Blendish
	drawBlendish(vgCtx, 10, 62, 600.0f, 420.0f, t);

	vg::popState(vgCtx);
}

static void renderBouncingEllipseDemo(vg::Context* vgCtx, float width, float height, vg::FontHandle fontHandle, float dt)
{
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
	} else if (pos[0] + size[0] * 0.5f > width) {
		pos[0] = width - size[0] * 0.5f;
		dir[0] *= -1.0f;
	}

	if (pos[1] - size[1] * 0.5f < 0.0f) {
		pos[1] = size[1] * 0.5f;
		dir[1] *= -1.0f;
	} else if (pos[1] + size[1] * 0.5f > height) {
		pos[1] = height - size[1] * 0.5f;
		dir[1] *= -1.0f;
	}

	vg::beginPath(vgCtx);
	vg::ellipse(vgCtx, pos[0], pos[1], size[0] * 0.5f, size[1] * 0.5f);
	vg::fillPath(vgCtx, vg::Colors::Red, vg::FillFlags::Convex);
	vg::strokePath(vgCtx, vg::Colors::Black, 4.0f, vg::StrokeFlags::ButtMiterAA);

	vg::TextConfig txtCfg = vg::makeTextConfig(vgCtx, fontHandle, 20.0f, vg::TextAlign::MiddleCenter, vg::Colors::Black);
	vg::text(vgCtx, txtCfg, pos[0], pos[1], u8"Hello World\u2026", NULL);
}

static inline const char* svgSkipWhitespace(const char* ptr, const char* end)
{
	while (ptr != end && bx::isSpace(*ptr) && *ptr != '\0') {
		++ptr;
	}

	return ptr;
}

static inline const char* svgSkipCommaWhitespace(const char* ptr, const char* end)
{
	// comma-wsp: (wsp+ comma? wsp*) | (comma wsp*)
	ptr = svgSkipWhitespace(ptr, end);
	if (*ptr == ',') {
		ptr = svgSkipWhitespace(ptr + 1, end);
	}

	return ptr;
}

static const char* svgParseCoord(const char* str, const char* end, float* coord)
{
	const char* ptr = svgSkipCommaWhitespace(str, end);

//	BX_CHECK(ptr != end && !bx::isAlpha(*ptr), "Parse error");

	char* coordEnd;
	*coord = strtof(ptr, &coordEnd);

//	BX_CHECK(coordEnd != NULL, "Failed to parse coordinate");
//	BX_CHECK(coordEnd <= end, "strtof() read past end of buffer");

	return svgSkipCommaWhitespace(coordEnd, end);
}

static const char* svgParseFlag(const char* str, const char* end, float* flag)
{
	const char* ptr = svgSkipCommaWhitespace(str, end);

//	BX_CHECK(ptr != end && !bx::isAlpha(*ptr), "Parse error");

	if (*ptr == '0') {
		*flag = 0.0f;
	} else {
		*flag = 1.0f;
	}

	return svgSkipCommaWhitespace(ptr + 1, end);
}

static float nsvg__vecang(float ux, float uy, float vx, float vy)
{
	const float umag = bx::sqrt(ux * ux + uy * uy);
	const float vmag = bx::sqrt(vx * vx + vy * vy);
	const float u_dot_v = ux * vx + uy * vy;
	const float r = bx::clamp<float>(u_dot_v / (umag * vmag), -1.0f, 1.0f);
	return bx::sign(ux * vy - uy * vx) * bx::acos(r);
}

// nsvg__pathArcTo(NSVGparser* p, float* cpx, float* cpy, float* args, int rel)
static void convertArcToBezier(vg::Context* ctx, vg::CommandListHandle cl, const float* arcToArgs, const float* lastPt)
{
	// Ported from canvg (https://code.google.com/p/canvg/)
	float rx = bx::abs(arcToArgs[0]);                    // x radius
	float ry = bx::abs(arcToArgs[1]);                    // y radius
	const float rotx = bx::toRad(arcToArgs[2]);          // x rotation angle
	const int fa = bx::abs(arcToArgs[3]) > 1e-6 ? 1 : 0; // Large arc
	const int fs = bx::abs(arcToArgs[4]) > 1e-6 ? 1 : 0; // Sweep direction
	const float x1 = lastPt[0];                          // start point x
	const float y1 = lastPt[1];                          // start point y
	const float x2 = arcToArgs[5];                       // end point x
	const float y2 = arcToArgs[6];                       // end point y

	float dx = x1 - x2;
	float dy = y1 - y2;
	float d = bx::sqrt(dx * dx + dy * dy);
	if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
		// The arc degenerates to a line
		vg::clLineTo(ctx, cl, x2, y2);
		return;
	}

	const float sinrx = bx::sin(rotx);
	const float cosrx = bx::cos(rotx);

	// Convert to center point parameterization.
	// http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
	// 1) Compute x1', y1'
	const float x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
	const float y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
	d = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
	if (d > 1) {
		d = bx::sqrt(d);
		rx *= d;
		ry *= d;
	}

	// 2) Compute cx', cy'
	float s = 0.0f;
	float sa = (rx * rx) * (ry * ry) - (rx * rx) * (y1p * y1p) - (ry * ry) * (x1p * x1p);
	const float sb = (rx * rx) * (y1p * y1p) + (ry * ry) * (x1p * x1p);
	if (sa < 0.0f) {
		sa = 0.0f;
	}
	if (sb > 0.0f) {
		s = bx::sqrt(sa / sb);
	}
	if (fa == fs) {
		s = -s;
	}
	const float cxp = s * rx * y1p / ry;
	const float cyp = s * -ry * x1p / rx;

	// 3) Compute cx,cy from cx',cy'
	const float cx = (x1 + x2) / 2.0f + cosrx * cxp - sinrx * cyp;
	const float cy = (y1 + y2) / 2.0f + sinrx * cxp + cosrx * cyp;

	// 4) Calculate theta1, and delta theta.
	const float ux = (x1p - cxp) / rx;
	const float uy = (y1p - cyp) / ry;
	const float vx = (-x1p - cxp) / rx;
	const float vy = (-y1p - cyp) / ry;
	const float a1 = nsvg__vecang(1.0f, 0.0f, ux, uy); // Initial angle

	float da = nsvg__vecang(ux, uy, vx, vy);     // Delta angle
	if (fs == 0 && da > 0) {
		da -= bx::kPi2;
	} else if (fs == 1 && da < 0) {
		da += bx::kPi2;
	}

	// Approximate the arc using cubic spline segments.
	float t[6];
	t[0] = cosrx;
	t[1] = sinrx;
	t[2] = -sinrx;
	t[3] = cosrx;
	t[4] = cx;
	t[5] = cy;

	// Split arc into max 90 degree segments.
	// The loop assumes an iteration per end point (including start and end), this +1.
	const int ndivs = (int)(bx::abs(da) / bx::kPiHalf + 1.0f);
	const float hda = (da / (float)ndivs) / 2.0f;
	float kappa = bx::abs(4.0f / 3.0f * (1.0f - bx::cos(hda)) / bx::sin(hda));
	if (da < 0.0f) {
		kappa = -kappa;
	}

	float px = 0.0f;
	float py = 0.0f;
	float ptanx = 0.0f;
	float ptany = 0.0f;

	for (int i = 0; i <= ndivs; i++) {
		const float a = a1 + da * ((float)i / (float)ndivs);
		dx = bx::cos(a);
		dy = bx::sin(a);

		const float dxrx = dx * rx;
		const float dyry = dy * ry;
		const float x = dxrx * t[0] + dyry * t[2] + t[4];
		const float y = dxrx * t[1] + dyry * t[3] + t[5];

		const float dyrxkappa = dy * rx * kappa;
		const float dxrykappa = dx * ry * kappa;
		const float tanx = dxrykappa * t[2] - dyrxkappa * t[0];
		const float tany = dxrykappa * t[3] - dyrxkappa * t[1];

		if (i > 0) {
			vg::clCubicTo(ctx, cl, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
		}

		px = x;
		py = y;
		ptanx = tanx;
		ptany = tany;
	}
}

// Borrowed from SimpleSVG (https://github.com/jdryg/simple-svg/blob/master/src/ssvg_parser.cpp)
static bool svgPathFromString(vg::Context* ctx, vg::CommandListHandle cl, const bx::StringView& str)
{
	const char* ptr = str.getPtr();
	const char* end = str.getTerm();
	float firstX = 0.0f;
	float firstY = 0.0f;
	float lastX = 0.0f;
	float lastY = 0.0f;
	float lastCPX = 0.0f;
	float lastCPY = 0.0f;
	char lastCommand = 0;

	vg::clBeginPath(ctx, cl);

	while (ptr != end) {
		char ch = *ptr;

//		BX_CHECK(!bx::isSpace(ch) && ch != ',', "Parse error");

		if (bx::isAlpha(ch)) {
			++ptr;
		} else {
			ch = lastCommand;
		}

		const char lch = bx::toLower(ch);

		if (lch == 'm') {
			// MoveTo
			float x, y;
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			if (ch == lch) {
				x += lastX;
				y += lastY;
			}

			firstX = lastX = x;
			firstY = lastY = y;

			vg::clMoveTo(ctx, cl, x, y);

			// https://www.w3.org/TR/SVG/paths.html#PathDataMovetoCommands
			// If a moveto is followed by multiple pairs of coordinates, the subsequent pairs are treated 
			// as implicit lineto commands. Hence, implicit lineto commands will be relative if the moveto 
			// is relative, and absolute if the moveto is absolute. If a relative moveto (m) appears as the 
			// first element of the path, then it is treated as a pair of absolute coordinates. In this case, 
			// subsequent pairs of coordinates are treated as relative even though the initial moveto is 
			// interpreted as an absolute moveto.
			ch = bx::isLower(ch) ? 'l' : 'L';
		} else if (lch == 'l') {
			// LineTo abs
			float x, y;
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			if (ch == lch) {
				x += lastX;
				y += lastY;
			}

			lastX = x;
			lastY = y;

			vg::clLineTo(ctx, cl, x, y);
		} else if (lch == 'h') {
			// Horizontal LineTo abs
			float x, y;
			ptr = svgParseCoord(ptr, end, &x);
			y = lastY;

			if (ch == lch) {
				x += lastX;
			}

			lastX = x;
			lastY = y;

			vg::clLineTo(ctx, cl, x, y);
		} else if (lch == 'v') {
			// Vertical LineTo abs
			float x, y;
			ptr = svgParseCoord(ptr, end, &y);
			x = lastX;

			if (ch == lch) {
				y += lastY;
			}

			lastX = x;
			lastY = y;

			vg::clLineTo(ctx, cl, x, y);
		} else if (lch == 'z') {
			// ClosePath
			lastX = firstX;
			lastY = firstY;
			// No data
			ptr = svgSkipCommaWhitespace(ptr, end);

			vg::clClosePath(ctx, cl);
		} else if (lch == 'c') {
			// CubicTo abs
			float c1x, c1y, c2x, c2y, x, y;
			ptr = svgParseCoord(ptr, end, &c1x);
			ptr = svgParseCoord(ptr, end, &c1y);
			ptr = svgParseCoord(ptr, end, &c2x);
			ptr = svgParseCoord(ptr, end, &c2y);
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			if (ch == lch) {
				c1x += lastX;
				c1y += lastY;
				c2x += lastX;
				c2y += lastY;
				x += lastX;
				y += lastY;
			}

			lastCPX = c2x;
			lastCPY = c2y;
			lastX = x;
			lastY = y;

			vg::clCubicTo(ctx, cl, c1x, c1y, c2x, c2y, x, y);
		} else if (lch == 's') {
			// CubicTo abs
			float c1x, c1y, c2x, c2y, x, y;
			ptr = svgParseCoord(ptr, end, &c2x);
			ptr = svgParseCoord(ptr, end, &c2y);
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			// The first control point is assumed to be the reflection of the second control point on 
			// the previous command relative to the current point. (If there is no previous command or 
			// if the previous command was not an C, c, S or s, assume the first control point is 
			// coincident with the current point.) 
			const char lastCmdLower = bx::toLower(lastCommand);
			if (lastCmdLower == 'c' || lastCmdLower == 's') {
				const float dx = lastX - lastCPX;
				const float dy = lastY - lastCPY;
				c1x = lastX + dx;
				c1y = lastY + dy;
			} else {
				c1x = lastX;
				c1y = lastY;
			}

			if (ch == lch) {
				c2x += lastX;
				c2y += lastY;
				x += lastX;
				y += lastY;
			}

			lastCPX = c2x;
			lastCPY = c2y;
			lastX = x;
			lastY = y;

			vg::clCubicTo(ctx, cl, c1x, c1y, c2x, c2y, x, y);
		} else if (lch == 'q') {
			// QuadraticTo abs
			float cx, cy, x, y;
			ptr = svgParseCoord(ptr, end, &cx);
			ptr = svgParseCoord(ptr, end, &cy);
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			if (ch == lch) {
				cx += lastX;
				cy += lastY;
				x += lastX;
				y += lastY;
			}

			lastCPX = cx;
			lastCPY = cy;
			lastX = x;
			lastY = y;

			vg::clQuadraticTo(ctx, cl, cx, cy, x, y);
		} else if (lch == 't') {
			// QuadraticTo abs
			float cx, cy, x, y;
			ptr = svgParseCoord(ptr, end, &x);
			ptr = svgParseCoord(ptr, end, &y);

			// The control point is assumed to be the reflection of the control point on the 
			// previous command relative to the current point. (If there is no previous command 
			// or if the previous command was not a Q, q, T or t, assume the control point is 
			// coincident with the current point.)
			const char lastCmdLower = bx::toLower(lastCommand);
			if (lastCmdLower == 'q' || lastCmdLower == 't') {
				const float dx = lastX - lastCPX;
				const float dy = lastY - lastCPY;
				cx = lastX + dx;
				cy = lastY + dy;
			} else {
				cx = lastX;
				cy = lastY;
			}

			if (ch == lch) {
				x += lastX;
				y += lastY;
			}

			lastCPX = cx;
			lastCPY = cy;
			lastX = x;
			lastY = y;

			vg::clQuadraticTo(ctx, cl, cx, cy, x, y);
		} else if (lch == 'a') {
			// ArcTo abs
			float arcToParams[7];
			ptr = svgParseCoord(ptr, end, &arcToParams[0]);
			ptr = svgParseCoord(ptr, end, &arcToParams[1]);
			ptr = svgParseCoord(ptr, end, &arcToParams[2]);
			ptr = svgParseFlag(ptr, end, &arcToParams[3]);
			ptr = svgParseFlag(ptr, end, &arcToParams[4]);
			ptr = svgParseCoord(ptr, end, &arcToParams[5]);
			ptr = svgParseCoord(ptr, end, &arcToParams[6]);

			if (ch == lch) {
				arcToParams[5] += lastX;
				arcToParams[6] += lastY;
			}

			const float lastPt[2] = { lastX, lastY };
			convertArcToBezier(ctx, cl, arcToParams, lastPt);

			lastX = arcToParams[5];
			lastY = arcToParams[6];
		} else {
//			BX_WARN(false, "Encountered unknown path command");
			return false;
		}

		lastCommand = ch;
	}

	return true;
}

static void drawWhitePawn(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("m 22.5,9 c -2.21,0 -4,1.79 -4,4 0,0.89 0.29,1.71 0.78,2.38 C 17.33,16.5 16,18.59 16,21 c 0,2.03 0.94,3.84 2.41,5.03 C 15.41,27.09 11,31.58 11,39.5 H 34 C 34,31.58 29.59,27.09 26.59,26.03 28.06,24.84 29,23.03 29,21 29,18.59 27.67,16.5 25.72,15.38 26.21,14.71 26.5,13.89 26.5,13 c 0,-2.21 -1.79,-4 -4,-4 z");
	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));
}

static void drawBlackPawn(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("m 22.5,9 c -2.21,0 -4,1.79 -4,4 0,0.89 0.29,1.71 0.78,2.38 C 17.33,16.5 16,18.59 16,21 c 0,2.03 0.94,3.84 2.41,5.03 C 15.41,27.09 11,31.58 11,39.5 H 34 C 34,31.58 29.59,27.09 26.59,26.03 28.06,24.84 29,23.03 29,21 29,18.59 27.67,16.5 25.72,15.38 26.21,14.71 26.5,13.89 26.5,13 c 0,-2.21 -1.79,-4 -4,-4 z");
	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));
}

static void drawWhiteKnight(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 22,10 C 32.5,11 38.5,18 38,39 L 15,39 C 15,30 25,32.5 23,18");
	const bx::StringView path1("M 24,18 C 24.38,20.91 18.45,25.37 16,27 C 13,29 13.18,31.34 11,31 C 9.958,30.06 12.41,27.96 11,28 C 10,28 11.19,29.23 10,30 C 9,30 5.997,31 6,26 C 6,24 12,14 12,14 C 12,14 13.89,12.1 14,10.5 C 13.27,9.506 13.5,8.5 13.5,7.5 C 14.5,6.5 16.5,10 16.5,10 L 18.5,10 C 18.5,10 19.28,8.008 21,7 C 22,7 22,10 22,10");
	const bx::StringView path2("M 9.5 25.5 A 0.5 0.5 0 1 1 8.5,25.5 A 0.5 0.5 0 1 1 9.5 25.5 z");
	const bx::StringView path3("M 15 15.5 A 0.5 1.5 0 1 1  14,15.5 A 0.5 1.5 0 1 1  15 15.5 z");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.3f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	const float mtx[] = { 0.866f, 0.5f, -0.5f, 0.866f, 9.693f, -5.173f };
	vg::clTransformMult(ctx, cl, mtx, vg::TransformOrder::Post);
	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveNonZero);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	vg::clPopState(ctx, cl);
}

static void drawBlackKnight(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 22,10 C 32.5,11 38.5,18 38,39 L 15,39 C 15,30 25,32.5 23,18");
	const bx::StringView path1("M 24,18 C 24.38,20.91 18.45,25.37 16,27 C 13,29 13.18,31.34 11,31 C 9.958,30.06 12.41,27.96 11,28 C 10,28 11.19,29.23 10,30 C 9,30 5.997,31 6,26 C 6,24 12,14 12,14 C 12,14 13.89,12.1 14,10.5 C 13.27,9.506 13.5,8.5 13.5,7.5 C 14.5,6.5 16.5,10 16.5,10 L 18.5,10 C 18.5,10 19.28,8.008 21,7 C 22,7 22,10 22,10");
	const bx::StringView path2("M 9.5 25.5 A 0.5 0.5 0 1 1 8.5,25.5 A 0.5 0.5 0 1 1 9.5 25.5 z");
	const bx::StringView path3("M 15 15.5 A 0.5 1.5 0 1 1  14,15.5 A 0.5 1.5 0 1 1  15 15.5 z");
	const bx::StringView path4("M 24.55,10.4 L 24.1,11.85 L 24.6,12 C 27.75,13 30.25,14.49 32.5,18.75 C 34.75,23.01 35.75,29.06 35.25,39 L 35.2,39.5 L 37.45,39.5 L 37.5,39 C 38,28.94 36.62,22.15 34.25,17.66 C 31.88,13.17 28.46,11.02 25.06,10.5 L 24.55,10.4 z ");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.3f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	const float mtx[] = { 0.866f, 0.5f, -0.5f, 0.866f, 9.693f, -5.173f };
	vg::clPushState(ctx, cl);
	vg::clTransformMult(ctx, cl, mtx, vg::TransformOrder::Post);
	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));
	vg::clPopState(ctx, cl);

	svgPathFromString(ctx, cl, path4);
	vg::clFillPath(ctx, cl, vg::Colors::White, VG_FILL_FLAGS(vg::PathType::Concave, vg::FillRule::EvenOdd, aa));

	vg::clPopState(ctx, cl);
}

static void drawWhiteBishop(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,36 C 12.39,35.03 19.11,36.43 22.5,34 C 25.89,36.43 32.61,35.03 36,36 C 36,36 37.65,36.54 39,38 C 38.32,38.97 37.35,38.99 36,38.5 C 32.61,37.53 25.89,38.96 22.5,37.5 C 19.11,38.96 12.39,37.53 9,38.5 C 7.65,38.99 6.68,38.97 6,38 C 7.35,36.54 9,36 9,36 z");
	const bx::StringView path1("M 15,32 C 17.5,34.5 27.5,34.5 30,32 C 30.5,30.5 30,30 30,30 C 30,27.5 27.5,26 27.5,26 C 33,24.5 33.5,14.5 22.5,10.5 C 11.5,14.5 12,24.5 17.5,26 C 17.5,26 15,27.5 15,30 C 15,30 14.5,30.5 15,32 z");
	const bx::StringView path2("M 25 8 A 2.5 2.5 0 1 1  20,8 A 2.5 2.5 0 1 1  25 8 z");
	const bx::StringView path3("M 17.5,26 L 27.5,26 M 15,30 L 30,30 M 22.5,15.5 L 22.5,20.5 M 20,18 L 25,18");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.6f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	vg::clPopState(ctx, cl);
}

static void drawBlackBishop(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,36 C 12.39,35.03 19.11,36.43 22.5,34 C 25.89,36.43 32.61,35.03 36,36 C 36,36 37.65,36.54 39,38 C 38.32,38.97 37.35,38.99 36,38.5 C 32.61,37.53 25.89,38.96 22.5,37.5 C 19.11,38.96 12.39,37.53 9,38.5 C 7.65,38.99 6.68,38.97 6,38 C 7.35,36.54 9,36 9,36 z");
	const bx::StringView path1("M 15,32 C 17.5,34.5 27.5,34.5 30,32 C 30.5,30.5 30,30 30,30 C 30,27.5 27.5,26 27.5,26 C 33,24.5 33.5,14.5 22.5,10.5 C 11.5,14.5 12,24.5 17.5,26 C 17.5,26 15,27.5 15,30 C 15,30 14.5,30.5 15,32 z");
	const bx::StringView path2("M 25 8 A 2.5 2.5 0 1 1  20,8 A 2.5 2.5 0 1 1  25 8 z");
	const bx::StringView path3("M 17.5,26 L 27.5,26 M 15,30 L 30,30 M 22.5,15.5 L 22.5,20.5 M 20,18 L 25,18");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.6f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	vg::clPopState(ctx, cl);
}

static void drawWhiteRook(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,39 L 36,39 L 36,36 L 9,36 L 9,39 z ");
	const bx::StringView path1("M 12,36 L 12,32 L 33,32 L 33,36 L 12,36 z ");
	const bx::StringView path2("M 11,14 L 11,9 L 15,9 L 15,11 L 20,11 L 20,9 L 25,9 L 25,11 L 30,11 L 30,9 L 34,9 L 34,14");
	const bx::StringView path3("M 34,14 L 31,17 L 14,17 L 11,14");
	const bx::StringView path4("M 31,17 L 31,29.5 L 14,29.5 L 14,17");
	const bx::StringView path5("M 31,29.5 L 32.5,32 L 12.5,32 L 14,29.5");
	const bx::StringView path6("M 11,14 L 34,14");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.3f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path4);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path5);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path6);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));

	vg::clPopState(ctx, cl);
}

static void drawBlackRook(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,39 L 36,39 L 36,36 L 9,36 L 9,39 z ");
	const bx::StringView path1("M 12.5,32 L 14,29.5 L 31,29.5 L 32.5,32 L 12.5,32 z ");
	const bx::StringView path2("M 12,36 L 12,32 L 33,32 L 33,36 L 12,36 z ");
	const bx::StringView path3("M 14,29.5 L 14,16.5 L 31,16.5 L 31,29.5 L 14,29.5 z ");
	const bx::StringView path4("M 14,16.5 L 11,14 L 34,14 L 31,16.5 L 14,16.5 z ");
	const bx::StringView path5("M 11,14 L 11,9 L 15,9 L 15,11 L 20,11 L 20,9 L 25,9 L 25,11 L 30,11 L 30,9 L 34,9 L 34,14 L 11,14 z ");
	const bx::StringView path6("M 12,35.5 L 33,35.5 L 33,35.5");
	const bx::StringView path7("M 13,31.5 L 32,31.5");
	const bx::StringView path8("M 14,29.5 L 31,29.5");
	const bx::StringView path9("M 14,16.5 L 31,16.5");
	const bx::StringView path10("M 11,14 L 34,14");

	vg::clPushState(ctx, cl);
	vg::clTransformTranslate(ctx, cl, 0.0f, 0.3f);

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path4);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path5);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path6);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path7);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path8);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path9);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path10);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	vg::clPopState(ctx, cl);
}

static void drawWhiteQueen(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,26 C 17.5,24.5 30,24.5 36,26 L 38.5,13.5 L 31,25 L 30.7,10.9 L 25.5,24.5 L 22.5,10 L 19.5,24.5 L 14.3,10.9 L 14,25 L 6.5,13.5 L 9,26 z");
	const bx::StringView path1("M 9,26 C 9,28 10.5,28 11.5,30 C 12.5,31.5 12.5,31 12,33.5 C 10.5,34.5 11,36 11,36 C 9.5,37.5 11,38.5 11,38.5 C 17.5,39.5 27.5,39.5 34,38.5 C 34,38.5 35.5,37.5 34,36 C 34,36 34.5,34.5 33,33.5 C 32.5,31 32.5,31.5 33.5,30 C 34.5,28 36,28 36,26 C 27.5,24.5 17.5,24.5 9,26 z");
	const bx::StringView path2("M 11.5,30 C 15,29 30,29 33.5,30");
	const bx::StringView path3("M 12,33.5 C 18,32.5 27,32.5 33,33.5");

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Bevel, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Bevel, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Bevel, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Bevel, aa));

	vg::clBeginPath(ctx, cl);
	vg::clCircle(ctx, cl, 6.0f, 12.0f, 2.0f);
	vg::clCircle(ctx, cl, 14.0f, 9.0f, 2.0f);
	vg::clCircle(ctx, cl, 22.5f, 8.0f, 2.0f);
	vg::clCircle(ctx, cl, 31.0f, 9.0f, 2.0f);
	vg::clCircle(ctx, cl, 39.0f, 12.0f, 2.0f);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::Convex);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Bevel, aa));
}

static void drawBlackQueen(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 9,26 C 17.5,24.5 30,24.5 36,26 L 38.5,13.5 L 31,25 L 30.7,10.9 L 25.5,24.5 L 22.5,10 L 19.5,24.5 L 14.3,10.9 L 14,25 L 6.5,13.5 L 9,26 z");
	const bx::StringView path1("m 9,26 c 0,2 1.5,2 2.5,4 1,1.5 1,1 0.5,3.5 -1.5,1 -1,2.5 -1,2.5 -1.5,1.5 0,2.5 0,2.5 6.5,1 16.5,1 23,0 0,0 1.5,-1 0,-2.5 0,0 0.5,-1.5 -1,-2.5 -0.5,-2.5 -0.5,-2 0.5,-3.5 1,-2 2.5,-2 2.5,-4 -8.5,-1.5 -18.5,-1.5 -27,0 z");
	const bx::StringView path2("M 11.5,30 C 15,29 30,29 33.5,30");
	const bx::StringView path3("m 12,33.5 c 6,-1 15,-1 21,0");
	const bx::StringView path4("M 11,38.5 A 35,35 1 0 0 34,38.5");
	const bx::StringView path5("M 11,29 A 35,35 1 0 1 34,29");
	const bx::StringView path6("M 12.5,31.5 L 32.5,31.5");
	const bx::StringView path7("M 11.5,34.5 A 35,35 1 0 0 33.5,34.5");
	const bx::StringView path8("M 10.5,37.5 A 35,35 1 0 0 34.5,37.5");

	svgPathFromString(ctx, cl, path0);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::Concave);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	vg::clBeginPath(ctx, cl);
	vg::clCircle(ctx, cl, 6.0f, 12.0f, 2.0f);
	vg::clCircle(ctx, cl, 14.0f, 9.0f, 2.0f);
	vg::clCircle(ctx, cl, 22.5f, 8.0f, 2.0f);
	vg::clCircle(ctx, cl, 31.0f, 9.0f, 2.0f);
	vg::clCircle(ctx, cl, 39.0f, 12, 2.0f);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::Convex);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path4);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path5);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path6);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path7);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path8);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));
}

static void drawWhiteKing(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 22.5,11.63 L 22.5,6");
	const bx::StringView path1("M 20,8 L 25,8");
	const bx::StringView path2("M 22.5,25 C 22.5,25 27,17.5 25.5,14.5 C 25.5,14.5 24.5,12 22.5,12 C 20.5,12 19.5,14.5 19.5,14.5 C 18,17.5 22.5,25 22.5,25");
	const bx::StringView path3("M 12.5,37 C 18,40.5 27,40.5 32.5,37 L 32.5,30 C 32.5,30 41.5,25.5 38.5,19.5 C 34.5,13 25,16 22.5,23.5 L 22.5,27 L 22.5,23.5 C 20,16 10.5,13 6.5,19.5 C 3.5,25.5 12.5,30 12.5,30 L 12.5,37");
	const bx::StringView path4("M 12.5,30 C 18,27 27,27 32.5,30");
	const bx::StringView path5("M 12.5,33.5 C 18,30.5 27,30.5 32.5,33.5");
	const bx::StringView path6("M 12.5,37 C 18,34 27,34 32.5,37");

	svgPathFromString(ctx, cl, path0);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clFillPath(ctx, cl, vg::Colors::White, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path4);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path5);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path6);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));
}

static void drawBlackKing(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa)
{
	const bx::StringView path0("M 22.5,11.63 L 22.5,6");
	const bx::StringView path1("M 22.5,25 C 22.5,25 27,17.5 25.5,14.5 C 25.5,14.5 24.5,12 22.5,12 C 20.5,12 19.5,14.5 19.5,14.5 C 18,17.5 22.5,25 22.5,25");
	const bx::StringView path2("M 12.5,37 C 18,40.5 27,40.5 32.5,37 L 32.5,30 C 32.5,30 41.5,25.5 38.5,19.5 C 34.5,13 25,16 22.5,23.5 L 22.5,27 L 22.5,23.5 C 20,16 10.5,13 6.5,19.5 C 3.5,25.5 12.5,30 12.5,30 L 12.5,37");
	const bx::StringView path3("M 20,8 L 25,8");
	const bx::StringView path4("M 32,29.5 C 32,29.5 40.5,25.5 38.03,19.85 C 34.15,14 25,18 22.5,24.5 L 22.5,26.6 L 22.5,24.5 C 20,18 10.85,14 6.97,19.85 C 4.5,25.5 13,29.5 13,29.5");
	const bx::StringView path5("M 12.5,30 C 18,27 27,27 32.5,30 M 12.5,33.5 C 18,30.5 27,30.5 32.5,33.5 M 12.5,37 C 18,34 27,34 32.5,37");

	svgPathFromString(ctx, cl, path0);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path1);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path2);
	vg::clFillPath(ctx, cl, vg::Colors::Black, vg::FillFlags::ConcaveEvenOdd);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path3);
	vg::clStrokePath(ctx, cl, vg::Colors::Black, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Miter, aa));

	svgPathFromString(ctx, cl, path4);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));

	svgPathFromString(ctx, cl, path5);
	vg::clStrokePath(ctx, cl, vg::Colors::White, 1.5f, VG_STROKE_FLAGS(vg::LineCap::Round, vg::LineJoin::Round, aa));
}

static void renderChessboardDemo(vg::Context* ctx, const entry::MouseState* mouseState, float width, float height, bool clipPath, float t, bool aa, ChessboardDemoData* data)
{
	static const float kCanvasScaleDelta = 1.25f;
	static const float kCanvasMinScale = bx::pow(kCanvasScaleDelta, -8.0f);
	static const float kCanvasMaxScale = bx::pow(kCanvasScaleDelta, 12.0f);
	static const float kPieceUnitScaleSize = 45.0f; // Dimensions of SVG images
	static const vg::Color kCheckerboardColors[2] = {
		vg::color4ub(242, 232, 231, 255),
		vg::color4ub(163, 82, 78, 255)
	};
	static const vg::Color kTextColor = vg::color4ub(87, 30, 36, 255);

	static float canvasOrigin[2] = { -width * 0.5f, -height * 0.5f };
	static float canvasScale = 1.0f;

	static float prevMousePosSS[2] = { (float)mouseState->m_mx, (float)mouseState->m_my };
	static int32_t prevMouseZ = mouseState->m_mz;

	const float curMousePosSS[2] = { (float)mouseState->m_mx, (float)mouseState->m_my };

	if (mouseState->m_buttons[entry::MouseButton::Right]) {
		const float mouseDeltaSS[2] = {
			curMousePosSS[0] - prevMousePosSS[0],
			curMousePosSS[1] - prevMousePosSS[1]
		};

		canvasOrigin[0] = (canvasOrigin[0] - (mouseDeltaSS[0] / canvasScale));
		canvasOrigin[1] = (canvasOrigin[1] - (mouseDeltaSS[1] / canvasScale));
	}

	if (prevMouseZ != mouseState->m_mz) {
		const int32_t delta = mouseState->m_mz - prevMouseZ;

		const float oldScale = canvasScale;
		const float newScale = delta > 0
			? bx::min<float>(oldScale * kCanvasScaleDelta, kCanvasMaxScale)
			: bx::max<float>(oldScale / kCanvasScaleDelta, kCanvasMinScale)
			;

		const float scaleRatio = (newScale - oldScale) / (newScale * oldScale);
		canvasOrigin[0] = (canvasOrigin[0] + curMousePosSS[0] * scaleRatio);
		canvasOrigin[1] = (canvasOrigin[1] + curMousePosSS[1] * scaleRatio);

		canvasScale = newScale;
	}

	prevMousePosSS[0] = curMousePosSS[0];
	prevMousePosSS[1] = curMousePosSS[1];
	prevMouseZ = mouseState->m_mz;

	if (clipPath) {
		const float cx = width * 0.5f + 100.0f * bx::sin(t);
		const float cy = height * 0.5f + 100.0f * bx::cos(t);
		const float r = 100.0f;

		// Begin path clipping. All shapes rendered inside the beginClip()/endClip() block will
		// be used as the clip path for subsequent shapes.
		vg::beginClip(ctx, vg::ClipRule::In);
		vg::beginPath(ctx);
		vg::circle(ctx, cx, cy, r);
		vg::fillPath(ctx, vg::Colors::Black, vg::FillFlags::Convex);
		vg::endClip(ctx);

		// All shapes rendered from now on (until resetClip() is called) will be clipped by the
		// above clip shape.
	}

	vg::pushState(ctx);
	vg::transformIdentity(ctx);
	vg::setViewBox(ctx, canvasOrigin[0], canvasOrigin[1], width / canvasScale, height / canvasScale);

	// Render chessboard around (0, 0).
	{
		const float x0 = -4.0f * kPieceUnitScaleSize;
		const float y0 = -4.0f * kPieceUnitScaleSize;

		vg::beginPath(ctx);
		vg::rect(ctx, x0 - kPieceUnitScaleSize, y0 - kPieceUnitScaleSize, kPieceUnitScaleSize * 10, kPieceUnitScaleSize * 10);
		vg::fillPath(ctx, kCheckerboardColors[0], vg::FillFlags::ConvexAA);

		for (uint32_t y = 0; y < 8; ++y) {
			for (uint32_t x = 0; x < 8; ++x) {
				vg::beginPath(ctx);
				vg::rect(ctx, x0 + x * kPieceUnitScaleSize, y0 + y * kPieceUnitScaleSize, kPieceUnitScaleSize, kPieceUnitScaleSize);
				vg::fillPath(ctx, kCheckerboardColors[(x + y) % 2], vg::FillFlags::ConvexAA);
			}
		}

		vg::beginPath(ctx);
		vg::rect(ctx, x0, y0, kPieceUnitScaleSize * 8, kPieceUnitScaleSize * 8);
		vg::strokePath(ctx, kCheckerboardColors[1], 1.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));

		// Draw text at the border of the board
		{
			const vg::TextConfig txtCfg = vg::makeTextConfig(ctx, data->m_Font, 32.0f, vg::TextAlign::MiddleCenter, kTextColor);

			// Row numbers
			{
				const float l_x = x0 - kPieceUnitScaleSize * 0.5f;
				const float r_x = x0 + kPieceUnitScaleSize * 8.5f;
				static const char* labels[] = { "8", "7", "6", "5", "4", "3", "2", "1" };
				for (uint32_t y = 0; y < 8; ++y) {
					vg::text(ctx, txtCfg, l_x, y0 + kPieceUnitScaleSize * (y + 0.5f), labels[y], NULL);
					vg::text(ctx, txtCfg, r_x, y0 + kPieceUnitScaleSize * (y + 0.5f), labels[y], NULL);
				}
			}

			// Column letters
			{
				const float t_y = y0 - kPieceUnitScaleSize * 0.5f;
				const float b_y = y0 + kPieceUnitScaleSize * 8.5f;
				static const char* labels[] = { "a", "b", "c", "d", "e", "f", "g", "h" };
				for (uint32_t x = 0; x < 8; ++x) {
					vg::text(ctx, txtCfg, x0 + kPieceUnitScaleSize * (x + 0.5f), t_y, labels[x], NULL);
					vg::text(ctx, txtCfg, x0 + kPieceUnitScaleSize * (x + 0.5f), b_y, labels[x], NULL);
				}
			}
		}
	}

	// Render white pieces
	{
		static const ChessPiece::Enum pieceOrder[16] = {
			ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn,
			ChessPiece::Rook, ChessPiece::Knight, ChessPiece::Bishop, ChessPiece::Queen, ChessPiece::King, ChessPiece::Bishop, ChessPiece::Knight, ChessPiece::Rook
		};

		const float x0 = -4.0f * kPieceUnitScaleSize;
		const float y0 = +2.0f * kPieceUnitScaleSize;
		for (uint32_t y = 0; y < 2; ++y) {
			for (uint32_t x = 0; x < 8; ++x) {
				vg::pushState(ctx);
				vg::transformTranslate(ctx, x0 + kPieceUnitScaleSize * x, y0 + kPieceUnitScaleSize * y);
				vg::submitCommandList(ctx, data->m_WhitePieces[pieceOrder[x + y * 8]]);
				vg::popState(ctx);
			}
		}
	}

	// Render black pieces
	{
		static const ChessPiece::Enum pieceOrder[16] = {
			ChessPiece::Rook, ChessPiece::Knight, ChessPiece::Bishop, ChessPiece::Queen, ChessPiece::King, ChessPiece::Bishop, ChessPiece::Knight, ChessPiece::Rook,
			ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn, ChessPiece::Pawn
		};

		const float x0 = -4.0f * kPieceUnitScaleSize;
		const float y0 = -4.0f * kPieceUnitScaleSize;
		for (uint32_t y = 0; y < 2; ++y) {
			for (uint32_t x = 0; x < 8; ++x) {
				vg::pushState(ctx);
				vg::transformTranslate(ctx, x0 + kPieceUnitScaleSize * x, y0 + kPieceUnitScaleSize * y);
				vg::submitCommandList(ctx, data->m_BlackPieces[pieceOrder[x + y * 8]]);
				vg::popState(ctx);
			}
		}
	}

	vg::popState(ctx);

	if (clipPath) {
		// Reset clipping. All shapes rendered after this command will not be clipped.
		vg::resetClip(ctx);

		// Render a border around the clip path.
		const float cx = width * 0.5f + 100.0f * bx::sin(t);
		const float cy = height * 0.5f + 100.0f * bx::cos(t);
		const float r = 100.0f;
		vg::beginPath(ctx);
		vg::circle(ctx, cx, cy, r);
		vg::strokePath(ctx, vg::Colors::Black, 4.0f, VG_STROKE_FLAGS(vg::LineCap::Butt, vg::LineJoin::Miter, aa));
	}
}

static void loadChessPieces(vg::Context* ctx, ChessboardDemoData* data, uint32_t commandListFlags, bool aa)
{
	typedef void (*drawPieceFunc)(vg::Context* ctx, vg::CommandListHandle cl, uint32_t aa);

	// White pieces
	{
		vg::CommandListHandle* pieces = data->m_WhitePieces;
		static const drawPieceFunc drawFuncs[] = {
			drawWhiteKing,
			drawWhiteQueen,
			drawWhiteRook,
			drawWhiteBishop,
			drawWhiteKnight,
			drawWhitePawn
		};

		for (uint32_t i = 0; i < 6; ++i) {
			vg::CommandListHandle cl = vg::createCommandList(ctx, commandListFlags);

			drawFuncs[i](ctx, cl, aa ? 1 : 0);

			if (vg::isValid(pieces[i])) {
				vg::destroyCommandList(ctx, pieces[i]);
			}
			pieces[i] = cl;
		}
	}

	// Black pieces
	{
		vg::CommandListHandle* pieces = data->m_BlackPieces;
		static const drawPieceFunc drawFuncs[] = {
			drawBlackKing,
			drawBlackQueen,
			drawBlackRook,
			drawBlackBishop,
			drawBlackKnight,
			drawBlackPawn
		};

		for (uint32_t i = 0; i < 6; ++i) {
			vg::CommandListHandle cl = vg::createCommandList(ctx, commandListFlags);

			drawFuncs[i](ctx, cl, aa ? 1 : 0);

			if (vg::isValid(pieces[i])) {
				vg::destroyCommandList(ctx, pieces[i]);
			}
			pieces[i] = cl;
		}
	}
}

class ExampleVGRenderer : public entry::AppI
{
public:
	ExampleVGRenderer(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_vgCtx(NULL)
		, m_SelectedDemo(Demo::Chessboard)
		, m_NanoVGDemoBlowup(false)
		, m_ChessboardDemoTessCaching(true)
		, m_ChessboardDemoClipping(false)
		, m_ChessboardDemoAA(true)
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
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH|BGFX_CLEAR_STENCIL
			, 0x303030ff
			, 1.0f
			, 0
			);

		// Sequential view mode is required by vg-renderer.
		bgfx::setViewMode(0, bgfx::ViewMode::Sequential);

		imguiCreate();

		const vg::ContextConfig cfg = {
			256,                             // m_MaxGradients
			256,                             // m_MaxImagePatterns
			8,                               // m_MaxFonts
			32,                              // m_MaxStateStackSize
			64,                              // m_MaxImages
			256,                             // m_MaxCommandLists
			65536,                           // m_MaxVBVertices
			vg::ImageFlags::Filter_Bilinear, // m_FontAtlasImageFlags
			16                               // m_MaxCommandListDepth
		};
		m_vgCtx = vg::createContext(entry::getAllocator(), &cfg);
		if (!m_vgCtx) {
			bx::debugPrintf("Failed to create vg-renderer context.\n");
			return;
		}

		m_SansFontHandle = createFont(m_vgCtx, "sans", "font/roboto-regular.ttf");
		if (!vg::isValid(m_SansFontHandle)) {
			bx::debugPrintf("Failed to load font.\n");
		}
		m_SansBoldFontHandle = createFont(m_vgCtx, "sans-bold", "font/roboto-bold.ttf");
		if (!vg::isValid(m_SansBoldFontHandle)) {
			bx::debugPrintf("Failed to load font.\n");
		}
		m_IconsFontHandle = createFont(m_vgCtx, "icons", "font/entypo.ttf");
		if (!vg::isValid(m_IconsFontHandle)) {
			bx::debugPrintf("Failed to load font.\n");
		}

		// Load NanoVG demo data
		{
			m_NanoVGDemoData.fontNormal = m_SansFontHandle;
			m_NanoVGDemoData.fontBold = m_SansBoldFontHandle;
			m_NanoVGDemoData.fontIcons = m_IconsFontHandle;
			m_NanoVGDemoData.fontEmoji = VG_INVALID_HANDLE;
			for (uint32_t i = 0; i < BX_COUNTOF(m_NanoVGDemoData.images); ++i) {
				char file[128];
				bx::snprintf(file, 128, "images/image%d.jpg", i + 1);
				m_NanoVGDemoData.images[i] = createImage(m_vgCtx, file, 0);
				if (!vg::isValid(m_NanoVGDemoData.images[i])) {
					bx::debugPrintf("Could not load %s.\n", file);
				}
			}

			bndSetFont(createFont(m_vgCtx, "droidsans", "font/droidsans.ttf"));
			bndSetIconImage(createImage(m_vgCtx, "images/blender_icons16.png", 0));
		}

		// Load Chessboard demo data
		{
			for (uint32_t i = 0; i < 6; ++i) {
				m_ChessboardDemoData.m_WhitePieces[i] = VG_INVALID_HANDLE;
				m_ChessboardDemoData.m_BlackPieces[i] = VG_INVALID_HANDLE;
			}

			loadChessPieces(m_vgCtx, &m_ChessboardDemoData, m_ChessboardDemoTessCaching ? vg::CommandListFlags::Cacheable : 0, m_ChessboardDemoAA);

			m_ChessboardDemoData.m_Font = m_SansFontHandle;
		}

		m_timeOffset = bx::getHPCounter();
		m_LastFrameTimeOffset = m_timeOffset;
	}

	virtual int shutdown() override
	{
		// Destroy NanoVGDemoData
		{
			for (uint32_t i = 0; i < BX_COUNTOF(m_NanoVGDemoData.images); ++i) {
				if (vg::isValid(m_NanoVGDemoData.images[i])) {
					vg::destroyImage(m_vgCtx, m_NanoVGDemoData.images[i]);
					m_NanoVGDemoData.images[i] = VG_INVALID_HANDLE;
				}
			}
		}

		// Destroy ChessboardDemoData
		{
			for (uint32_t i = 0; i < 6; ++i) {
				if (vg::isValid(m_ChessboardDemoData.m_WhitePieces[i])) {
					vg::destroyCommandList(m_vgCtx, m_ChessboardDemoData.m_WhitePieces[i]);
					m_ChessboardDemoData.m_WhitePieces[i] = VG_INVALID_HANDLE;
				}

				if (vg::isValid(m_ChessboardDemoData.m_BlackPieces[i])) {
					vg::destroyCommandList(m_vgCtx, m_ChessboardDemoData.m_BlackPieces[i]);
					m_ChessboardDemoData.m_BlackPieces[i] = VG_INVALID_HANDLE;
				}
			}
		}

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

			const int64_t now = bx::getHPCounter();
			const double freq = double(bx::getHPFrequency());
			const float time = (float)((now - m_timeOffset) / freq);
			const float dt = (float)((now - m_LastFrameTimeOffset) / freq);
			m_LastFrameTimeOffset = now;

			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			vg::begin(m_vgCtx, 0, uint16_t(m_width), uint16_t(m_height), 1.0f);
			{
				if (m_SelectedDemo == Demo::BouncingEllipse) {
					renderBouncingEllipseDemo(m_vgCtx, (float)m_width, (float)m_height, m_SansFontHandle, dt);
				} else if (m_SelectedDemo == Demo::NanoVGDemo) {
					renderNanoVGDemo(m_vgCtx, (float)m_mouseState.m_mx, (float)m_mouseState.m_my, (float)m_width, (float)m_height, time, m_NanoVGDemoBlowup ? 1 : 0, &m_NanoVGDemoData);
				} else if (m_SelectedDemo == Demo::Chessboard) {
					renderChessboardDemo(m_vgCtx, &m_mouseState, (float)m_width, (float)m_height, m_ChessboardDemoClipping, time, m_ChessboardDemoAA, &m_ChessboardDemoData);
				} else {
					vg::TextConfig txtCfg = vg::makeTextConfig(m_vgCtx, m_SansFontHandle, 20.0f, vg::TextAlign::MiddleCenter, vg::Colors::White);
					vg::text(m_vgCtx, txtCfg, (float)(m_width / 2), (float)(m_height / 2), "TODO: Demo not implemented yet.", NULL);
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
		ImGui::SetNextWindowSize(ImVec2(300.0f, 160.0f), ImGuiCond_FirstUseEver);

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

			if (m_SelectedDemo == Demo::NanoVGDemo) {
				ImGui::Checkbox("Blowup", &m_NanoVGDemoBlowup);
			} else if (m_SelectedDemo == Demo::Chessboard) {
				bool reloadPieces = false;
				reloadPieces = ImGui::Checkbox("Tesselation Caching", &m_ChessboardDemoTessCaching) || reloadPieces;
				reloadPieces = ImGui::Checkbox("Antialiasing", &m_ChessboardDemoAA) || reloadPieces;
				ImGui::Checkbox("Clip Path", &m_ChessboardDemoClipping);
				ImGui::TextWrapped("Hold right mouse button to pan.\nUse mouse wheel to zoom in and out.");

				if (reloadPieces) {
					loadChessPieces(m_vgCtx, &m_ChessboardDemoData, m_ChessboardDemoTessCaching ? vg::CommandListFlags::Cacheable : 0, m_ChessboardDemoAA);
				}
			}
		}
		ImGui::End();
	}

	vg::Context* m_vgCtx;
	vg::FontHandle m_SansFontHandle;
	vg::FontHandle m_SansBoldFontHandle;
	vg::FontHandle m_IconsFontHandle;
	Demo::Enum m_SelectedDemo;

	entry::MouseState m_mouseState;

	int64_t m_timeOffset;
	int64_t m_LastFrameTimeOffset;

	NanoVGDemoData m_NanoVGDemoData;
	bool m_NanoVGDemoBlowup;

	ChessboardDemoData m_ChessboardDemoData;
	bool m_ChessboardDemoTessCaching;
	bool m_ChessboardDemoClipping;
	bool m_ChessboardDemoAA;

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
