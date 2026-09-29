#include "graphics.h"
#include "lunasvg.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#ifdef LUNASVG_ENABLE_TEXT_SHAPING
#include <hb.h>
#include <hb-ot.h>
#endif

namespace lunasvg {

const Color Color::Black(0xFF000000);
const Color Color::White(0xFFFFFFFF);
const Color Color::Transparent(0x00000000);

const Rect Rect::Empty(0, 0, 0, 0);
const Rect Rect::Invalid(0, 0, -1, -1);
const Rect Rect::Infinite(-FLT_MAX / 2.f, -FLT_MAX / 2.f, FLT_MAX, FLT_MAX);

Rect::Rect(const Box& box)
    : x(box.x), y(box.y), w(box.w), h(box.h)
{
}

const Transform Transform::Identity(1, 0, 0, 1, 0, 0);

Transform::Transform()
{
    plutovg_matrix_init_identity(&m_matrix);
}

Transform::Transform(float a, float b, float c, float d, float e, float f)
{
    plutovg_matrix_init(&m_matrix, a, b, c, d, e, f);
}

Transform::Transform(const Matrix& matrix)
    : Transform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f)
{
}

Transform Transform::operator*(const Transform& transform) const
{
    plutovg_matrix_t result;
    plutovg_matrix_multiply(&result, &transform.m_matrix, &m_matrix);
    return result;
}

Transform& Transform::operator*=(const Transform& transform)
{
    return (*this = *this * transform);
}

Transform& Transform::multiply(const Transform& transform)
{
    return (*this *= transform);
}

Transform& Transform::translate(float tx, float ty)
{
    return multiply(translated(tx, ty));
}

Transform& Transform::scale(float sx, float sy)
{
    return multiply(scaled(sx, sy));
}

Transform& Transform::rotate(float angle, float cx, float cy)
{
    return multiply(rotated(angle, cx, cy));
}

Transform& Transform::shear(float shx, float shy)
{
    return multiply(sheared(shx, shy));
}

Transform& Transform::postMultiply(const Transform& transform)
{
    return (*this = transform * *this);
}

Transform& Transform::postTranslate(float tx, float ty)
{
    return postMultiply(translated(tx, ty));
}

Transform& Transform::postScale(float sx, float sy)
{
    return postMultiply(scaled(sx, sy));
}

Transform& Transform::postRotate(float angle, float cx, float cy)
{
    return postMultiply(rotated(angle, cx, cy));
}

Transform& Transform::postShear(float shx, float shy)
{
    return postMultiply(sheared(shx, shy));
}

Transform Transform::inverse() const
{
    plutovg_matrix_t inverse;
    plutovg_matrix_invert(&m_matrix, &inverse);
    return inverse;
}

Transform& Transform::invert()
{
    plutovg_matrix_invert(&m_matrix, &m_matrix);
    return *this;
}

void Transform::reset()
{
    plutovg_matrix_init_identity(&m_matrix);
}

Point Transform::mapPoint(float x, float y) const
{
    plutovg_matrix_map(&m_matrix, x, y, &x, &y);
    return Point(x, y);
}

Point Transform::mapPoint(const Point& point) const
{
    return mapPoint(point.x, point.y);
}

Rect Transform::mapRect(const Rect& rect) const
{
    if(!rect.isValid()) {
        return Rect::Invalid;
    }

    plutovg_rect_t result = {rect.x, rect.y, rect.w, rect.h};
    plutovg_matrix_map_rect(&m_matrix, &result, &result);
    return result;
}

float Transform::xScale() const
{
    return std::sqrt(m_matrix.a * m_matrix.a + m_matrix.b * m_matrix.b);
}

float Transform::yScale() const
{
    return std::sqrt(m_matrix.c * m_matrix.c + m_matrix.d * m_matrix.d);
}

bool Transform::parse(const char* data, size_t length)
{
    return plutovg_matrix_parse(&m_matrix, data, length);
}

Transform Transform::rotated(float angle, float cx, float cy)
{
    plutovg_matrix_t matrix;
    if(cx == 0.f && cy == 0.f) {
        plutovg_matrix_init_rotate(&matrix, PLUTOVG_DEG2RAD(angle));
    } else {
        plutovg_matrix_init_translate(&matrix, cx, cy);
        plutovg_matrix_rotate(&matrix, PLUTOVG_DEG2RAD(angle));
        plutovg_matrix_translate(&matrix, -cx, -cy);
    }

    return matrix;
}

Transform Transform::scaled(float sx, float sy)
{
    plutovg_matrix_t matrix;
    plutovg_matrix_init_scale(&matrix, sx, sy);
    return matrix;
}

Transform Transform::sheared(float shx, float shy)
{
    plutovg_matrix_t matrix;
    plutovg_matrix_init_shear(&matrix, PLUTOVG_DEG2RAD(shx), PLUTOVG_DEG2RAD(shy));
    return matrix;
}

Transform Transform::translated(float tx, float ty)
{
    plutovg_matrix_t matrix;
    plutovg_matrix_init_translate(&matrix, tx, ty);
    return matrix;
}

Path::Path(const Path& path)
    : m_data(plutovg_path_reference(path.data()))
{
}

Path::Path(Path&& path)
    : m_data(path.release())
{
}

Path::~Path()
{
    plutovg_path_destroy(m_data);
}

Path& Path::operator=(const Path& path)
{
    Path(path).swap(*this);
    return *this;
}

Path& Path::operator=(Path&& path)
{
    Path(std::move(path)).swap(*this);
    return *this;
}

void Path::moveTo(float x, float y)
{
    plutovg_path_move_to(ensure(), x, y);
}

void Path::lineTo(float x, float y)
{
    plutovg_path_line_to(ensure(), x, y);
}

void Path::quadTo(float x1, float y1, float x2, float y2)
{
    plutovg_path_quad_to(ensure(), x1, y1, x2, y2);
}

void Path::cubicTo(float x1, float y1, float x2, float y2, float x3, float y3)
{
    plutovg_path_cubic_to(ensure(), x1, y1, x2, y2, x3, y3);
}

void Path::arcTo(float rx, float ry, float xAxisRotation, bool largeArcFlag, bool sweepFlag, float x, float y)
{
    plutovg_path_arc_to(ensure(), rx, ry, PLUTOVG_DEG2RAD(xAxisRotation), largeArcFlag, sweepFlag, x, y);
}

void Path::close()
{
    plutovg_path_close(ensure());
}

void Path::addEllipse(float cx, float cy, float rx, float ry)
{
    plutovg_path_add_ellipse(ensure(), cx, cy, rx, ry);
}

void Path::addRoundRect(float x, float y, float w, float h, float rx, float ry)
{
    plutovg_path_add_round_rect(ensure(), x, y, w, h, rx, ry);
}

void Path::addRect(float x, float y, float w, float h)
{
    plutovg_path_add_rect(ensure(), x, y, w, h);
}

void Path::addEllipse(const Point& center, const Size& radii)
{
    addEllipse(center.x, center.y, radii.w, radii.h);
}

void Path::addRoundRect(const Rect& rect, const Size& radii)
{
    addRoundRect(rect.x, rect.y, rect.w, rect.h, radii.w, radii.h);
}

void Path::addRect(const Rect& rect)
{
    addRect(rect.x, rect.y, rect.w, rect.h);
}

void Path::reset()
{
    if(m_data == nullptr)
        return;
    if(isUnique()) {
        plutovg_path_reset(m_data);
    } else {
        plutovg_path_destroy(m_data);
        m_data = nullptr;
    }
}

Rect Path::boundingRect() const
{
    if(m_data == nullptr)
        return Rect::Empty;
    plutovg_rect_t extents;
    plutovg_path_extents(m_data, &extents, false);
    return extents;
}

bool Path::isEmpty() const
{
    if(m_data)
        return plutovg_path_get_elements(m_data, nullptr) == 0;
    return true;
}

bool Path::isUnique() const
{
    return plutovg_path_get_reference_count(m_data) == 1;
}

bool Path::parse(const char* data, size_t length)
{
    plutovg_path_reset(ensure());
    return plutovg_path_parse(m_data, data, length);
}

plutovg_path_t* Path::ensure()
{
    if(isNull()) {
        m_data = plutovg_path_create();
    } else if(!isUnique()) {
        plutovg_path_destroy(m_data);
        m_data = plutovg_path_clone(m_data);
    }

    return m_data;
}

PathIterator::PathIterator(const Path& path)
    : m_size(plutovg_path_get_elements(path.data(), &m_elements))
    , m_index(0)
{
}

PathCommand PathIterator::currentSegment(std::array<Point, 3>& points) const
{
    auto command = m_elements[m_index].header.command;
    switch(command) {
    case PLUTOVG_PATH_COMMAND_MOVE_TO:
        points[0] = m_elements[m_index + 1].point;
        break;
    case PLUTOVG_PATH_COMMAND_LINE_TO:
        points[0] = m_elements[m_index + 1].point;
        break;
    case PLUTOVG_PATH_COMMAND_CUBIC_TO:
        points[0] = m_elements[m_index + 1].point;
        points[1] = m_elements[m_index + 2].point;
        points[2] = m_elements[m_index + 3].point;
        break;
    case PLUTOVG_PATH_COMMAND_CLOSE:
        points[0] = m_elements[m_index + 1].point;
        break;
    }

    return PathCommand(command);
}

void PathIterator::next()
{
    m_index += m_elements[m_index].header.length;
}

FontFace::FontFace(plutovg_font_face_t* face)
    : m_face(plutovg_font_face_reference(face))
{
}

FontFace::FontFace(const void* data, size_t length, plutovg_destroy_func_t destroy_func, void* closure, int ttcindex)
    : m_face(plutovg_font_face_load_from_data(data, length, ttcindex, destroy_func, closure))
{
}

FontFace::FontFace(const char* filename, int ttcindex)
    : m_face(plutovg_font_face_load_from_file(filename, ttcindex))
{
}

FontFace::FontFace(const FontFace& face)
    : m_face(plutovg_font_face_reference(face.get()))
{
}

FontFace::FontFace(FontFace&& face)
    : m_face(face.release())
{
}

FontFace::~FontFace()
{
    plutovg_font_face_destroy(m_face);
}

FontFace& FontFace::operator=(const FontFace& face)
{
    FontFace(face).swap(*this);
    return *this;
}

FontFace& FontFace::operator=(FontFace&& face)
{
    FontFace(std::move(face)).swap(*this);
    return *this;
}

void FontFace::swap(FontFace& face)
{
    std::swap(m_face, face.m_face);
}

plutovg_font_face_t* FontFace::release()
{
    return std::exchange(m_face, nullptr);
}

bool FontFaceCache::addFontFace(const std::string& family, bool bold, bool italic, const FontFace& face)
{
    if(!face.isNull())
        plutovg_font_face_cache_add(m_cache, family.data(), bold, italic, face.get());
    return !face.isNull();
}

FontFace FontFaceCache::getFontFace(const std::string& family, int weight, bool italic) const
{
    if(auto face = plutovg_font_face_cache_match(m_cache, family.data(), weight, italic)) {
        return FontFace(face);
    }

    static const struct {
        const char* generic;
        const char* fallback;
    } generic_fallbacks[] = {
#if defined(__linux__)
        {"sans-serif", "DejaVu Sans"},
        {"serif", "DejaVu Serif"},
        {"monospace", "DejaVu Sans Mono"},
#else
        {"sans-serif", "Arial"},
        {"serif", "Times New Roman"},
        {"monospace", "Courier New"},
#endif
        {"cursive", "Comic Sans MS"},
        {"fantasy", "Impact"}
    };

    for(auto value : generic_fallbacks) {
        if(value.generic == family || family.empty()) {
            return FontFace(plutovg_font_face_cache_match(m_cache, value.fallback, weight, italic));
        }
    }

    return FontFace();
}

FontFaceCache::~FontFaceCache()
{
    plutovg_font_face_cache_destroy(m_cache);
}

void FontFaceCache::setFallbackFontFunc(lunasvg_fallback_font_func_t func, void* closure)
{
    m_fallbackFontClosure = closure;
    m_fallbackFontFunc = func;
}

void FontFaceCache::setTextImageFunc(lunasvg_text_image_func_t func, void* closure)
{
    m_textImageClosure = closure;
    m_textImageFunc = func;
}

bool FontFaceCache::getTextImage(const void* data, int index, const std::u32string_view& text, float size, int weight, bool italic, uint32_t color, float* advance, lunasvg_text_image_t* image) const
{
    auto func = m_textImageFunc.load();
    return func && func(data, index, reinterpret_cast<const uint32_t*>(text.data()), text.length(), size, weight, italic, color, advance, image, m_textImageClosure.load());
}

FontFace FontFaceCache::getFallbackFontFace(uint32_t codepoint, int weight, bool italic, const std::string& language, const std::string& family) const
{
    auto func = m_fallbackFontFunc.load();
    if(func == nullptr)
        return FontFace();
    auto fallbackFamily = func(codepoint, weight, italic, language.c_str(), family.c_str(), m_fallbackFontClosure.load());
    if(fallbackFamily == nullptr)
        return FontFace();
    if(auto face = plutovg_font_face_cache_match(m_cache, fallbackFamily, weight, italic))
        return FontFace(face);
    return FontFace();
}

FontFaceCache::FontFaceCache()
    : m_cache(plutovg_font_face_cache_create())
{
#ifndef LUNASVG_DISABLE_LOAD_SYSTEM_FONTS
    plutovg_font_face_cache_load_sys(m_cache);
#endif
}

FontFaceCache* fontFaceCache()
{
    static FontFaceCache cache;
    return &cache;
}

#ifdef LUNASVG_ENABLE_TEXT_SHAPING

// weight and style also select the instance of a variable font with weight, italic or slant axes
// one HarfBuzz face per font face, HarfBuzz keeps its shape plans and table caches on it; the face keeps the font
// face alive, so the pointer stays a valid key; the cache lives as long as the process, like the font face cache
static hb_face_t* referenceHbFace(plutovg_font_face_t* face)
{
    static auto& mutex = *new std::mutex;
    static auto& faces = *new std::unordered_map<plutovg_font_face_t*, hb_face_t*>;
    std::lock_guard lock(mutex);
    auto& hbFace = faces[face];
    if(hbFace == nullptr) {
        unsigned int length = 0;
        int ttcindex = 0;
        auto data = plutovg_font_face_get_data(face, &length, &ttcindex);
        auto destroy_func = [](void* closure) { plutovg_font_face_destroy(static_cast<plutovg_font_face_t*>(closure)); };
        auto blob = hb_blob_create(static_cast<const char*>(data), length, HB_MEMORY_MODE_READONLY, plutovg_font_face_reference(face), destroy_func);
        hbFace = hb_face_create(blob, ttcindex);
        hb_face_make_immutable(hbFace);
        hb_blob_destroy(blob);
    }

    return hb_face_reference(hbFace);
}

static std::shared_ptr<hb_font_t> createHbFont(plutovg_font_face_t* face, int weight, bool italic)
{
    auto hbFace = referenceHbFace(face);
    auto hbFont = hb_font_create(hbFace);
    std::vector<hb_variation_t> variations;
    hb_ot_var_axis_info_t axis;
    if(hb_ot_var_find_axis_info(hbFace, HB_TAG('w', 'g', 'h', 't'), &axis))
        variations.push_back({axis.tag, std::clamp<float>(weight, axis.min_value, axis.max_value)});
    if(italic && hb_ot_var_find_axis_info(hbFace, HB_TAG('i', 't', 'a', 'l'), &axis))
        variations.push_back({axis.tag, axis.max_value});
    else if(italic && hb_ot_var_find_axis_info(hbFace, HB_TAG('s', 'l', 'n', 't'), &axis))
        variations.push_back({axis.tag, std::clamp(-12.f, axis.min_value, axis.max_value)});
    if(!variations.empty())
        hb_font_set_variations(hbFont, variations.data(), variations.size());

    // like browsers, bold and oblique are synthesized when the font has no such face
    if(weight >= 600 && hb_style_get_value(hbFont, HB_STYLE_TAG_WEIGHT) < 600)
        hb_font_set_synthetic_bold(hbFont, 0.02f, 0.02f, false);
    if(italic && hb_style_get_value(hbFont, HB_STYLE_TAG_ITALIC) == 0.f && hb_style_get_value(hbFont, HB_STYLE_TAG_SLANT_RATIO) == 0.f)
        hb_font_set_synthetic_slant(hbFont, 0.25f);

    hb_face_destroy(hbFace);
    return std::shared_ptr<hb_font_t>(hbFont, hb_font_destroy);
}

static bool isClusterContinuation(hb_unicode_funcs_t* unicodeFuncs, hb_codepoint_t codepoint)
{
    switch(hb_unicode_general_category(unicodeFuncs, codepoint)) {
    case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
        return true;
    default:
        return false;
    }
}

static bool isCommonScript(hb_script_t script)
{
    return script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED || script == HB_SCRIPT_UNKNOWN;
}

// HarfBuzz hands out the images of sbix and CBDT whatever their format is, only PNG can be decoded
static bool hasPngImage(hb_font_t* font, hb_codepoint_t glyph)
{
    static const char signature[] = {'\x89', 'P', 'N', 'G'};
    auto png = hb_ot_color_glyph_reference_png(font, glyph);
    unsigned int length = 0;
    auto data = hb_blob_get_data(png, &length);
    auto isPng = length > sizeof(signature) && std::equal(signature, signature + sizeof(signature), data);
    hb_blob_destroy(png);
    return isPng;
}

static bool hasTable(hb_face_t* face, hb_tag_t tag)
{
    auto blob = hb_face_reference_table(face, tag);
    auto length = hb_blob_get_length(blob);
    hb_blob_destroy(blob);
    return length > 0;
}

// hvgl outlines and sbix images other than PNG, like the emjc of Apple Color Emoji, are only decoded by the
// platform, which then also shapes the text since such fonts can hide their layout tables
static bool isPlatformFont(hb_font_t* font, hb_codepoint_t codepoint)
{
    auto face = hb_font_get_face(font);
    if(hasTable(face, HB_TAG('h', 'v', 'g', 'l')))
        return true;
    hb_codepoint_t glyph = 0;
    return hasTable(face, HB_TAG('s', 'b', 'i', 'x')) && hb_font_get_nominal_glyph(font, codepoint, &glyph) && !hasPngImage(font, glyph);
}

static const void* fontData(hb_font_t* font, int& index)
{
    auto face = hb_font_get_face(font);
    auto blob = hb_face_reference_blob(face);
    auto data = hb_blob_get_data(blob, nullptr);
    hb_blob_destroy(blob);
    index = hb_face_get_index(face);
    return data;
}

static float measurePlatformText(hb_font_t* font, const std::u32string_view& text, const Font& style)
{
    int index = 0;
    auto data = fontData(font, index);
    auto advance = 0.f;
    fontFaceCache()->getTextImage(data, index, text, style.size(), style.weight(), style.italic(), 0, &advance, nullptr);
    return advance;
}

// `text` is a part of `context`, the characters around it shape its ends, like the joining of Arabic letters;
// runs in fonts that only the platform can draw go to `runFunc` with their position
template<typename GlyphFunc, typename RunFunc>
static float shapeText(const Font& font, const std::u32string_view& text, bool rightToLeft, const std::u32string_view& context, GlyphFunc glyphFunc, RunFunc runFunc)
{
    auto bufferText = context.empty() ? text : context;
    size_t textOffset = text.data() - bufferText.data();
    struct TextRun {
        size_t startOffset;
        size_t endOffset;
        hb_script_t script;
        size_t fontIndex;
    };

    auto hbFonts = font.hbFonts();
    std::vector<FontFace> fallbackFaces;
    auto unicodeFuncs = hb_unicode_funcs_get_default();
    // a character followed by the emoji variation selector asks for a color emoji font first, like browsers do
    auto findFontIndex = [&](hb_codepoint_t codepoint, const TextRun* lastRun, bool emojiPresentation) {
        if(lastRun && isClusterContinuation(unicodeFuncs, codepoint))
            return lastRun->fontIndex;
        // only the fonts of the font-family list come before the callback, it decides for the fallback fonts
        hb_codepoint_t glyph = 0;
        for(size_t index = 0; index < font.hbFonts().size() && !emojiPresentation; ++index) {
            if(hb_font_get_nominal_glyph(hbFonts[index].get(), codepoint, &glyph)) {
                return index;
            }
        }

        static const std::string emojiLanguage("und-Zsye");
        auto fallbackFace = fontFaceCache()->getFallbackFontFace(codepoint, font.weight(), font.italic(), emojiPresentation ? emojiLanguage : font.language(), font.family());
        if(fallbackFace.isNull() && emojiPresentation) {
            for(size_t index = 0; index < font.hbFonts().size(); ++index) {
                if(hb_font_get_nominal_glyph(hbFonts[index].get(), codepoint, &glyph)) {
                    return index;
                }
            }
        }

        if(!fallbackFace.isNull()) {
            for(size_t index = 0; index < fallbackFaces.size(); ++index) {
                if(fallbackFaces[index].get() == fallbackFace.get()) {
                    return font.hbFonts().size() + index;
                }
            }

            auto hbFont = createHbFont(fallbackFace.get(), font.weight(), font.italic());
            if(hb_font_get_nominal_glyph(hbFont.get(), codepoint, &glyph)) {
                fallbackFaces.push_back(std::move(fallbackFace));
                hbFonts.push_back(std::move(hbFont));
                return hbFonts.size() - 1;
            }
        }

        return lastRun ? lastRun->fontIndex : 0;
    };

    std::vector<TextRun> runs;
    for(size_t offset = 0; offset < text.length(); ++offset) {
        auto lastRun = runs.empty() ? nullptr : &runs.back();
        auto script = hb_unicode_script(unicodeFuncs, text[offset]);
        auto emojiPresentation = offset + 1 < text.length() && text[offset + 1] == 0xFE0F;
        auto fontIndex = findFontIndex(text[offset], lastRun, emojiPresentation);
        if(isCommonScript(script))
            script = lastRun ? lastRun->script : HB_SCRIPT_INVALID;
        if(lastRun && lastRun->fontIndex == fontIndex && (lastRun->script == script || lastRun->script == HB_SCRIPT_INVALID)) {
            lastRun->endOffset = offset + 1;
            lastRun->script = script;
        } else {
            runs.push_back({offset, offset + 1, script, fontIndex});
        }
    }

    if(rightToLeft)
        std::reverse(runs.begin(), runs.end());
    auto buffer = hb_buffer_create();
    auto advance = 0.f;
    for(const auto& run : runs) {
        auto hbFont = hbFonts[run.fontIndex].get();
        if(fontFaceCache()->hasTextImageFunc() && isPlatformFont(hbFont, text[run.startOffset])) {
            auto runText = text.substr(run.startOffset, run.endOffset - run.startOffset);
            runFunc(hbFont, runText, advance);
            advance += measurePlatformText(hbFont, runText, font);
            continue;
        }

        auto scale = font.size() / hb_face_get_upem(hb_font_get_face(hbFont));
        hb_buffer_clear_contents(buffer);
        hb_buffer_add_utf32(buffer, reinterpret_cast<const uint32_t*>(bufferText.data()), bufferText.length(), textOffset + run.startOffset, run.endOffset - run.startOffset);
        hb_buffer_set_script(buffer, run.script);
        hb_buffer_set_direction(buffer, rightToLeft ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        if(!font.language().empty())
            hb_buffer_set_language(buffer, hb_language_from_string(font.language().data(), font.language().size()));
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(hbFont, buffer, nullptr, 0);

        unsigned int count = 0;
        auto infos = hb_buffer_get_glyph_infos(buffer, &count);
        auto positions = hb_buffer_get_glyph_positions(buffer, &count);
        for(unsigned int i = 0; i < count; ++i) {
            glyphFunc(hbFont, infos[i].codepoint, advance + positions[i].x_offset * scale, positions[i].y_offset * scale, scale);
            advance += positions[i].x_advance * scale;
        }
    }

    hb_buffer_destroy(buffer);
    return advance;
}

struct GlyphOutline {
    plutovg_canvas_t* canvas;
    float x;
    float y;
    float xScale;
    float yScale;
};

static hb_draw_funcs_t* glyphDrawFuncs()
{
    static hb_draw_funcs_t* drawFuncs = [] {
        auto drawFuncs = hb_draw_funcs_create();
        hb_draw_funcs_set_move_to_func(drawFuncs, [](hb_draw_funcs_t*, void* data, hb_draw_state_t*, float x, float y, void*) {
            auto outline = static_cast<GlyphOutline*>(data);
            plutovg_canvas_move_to(outline->canvas, outline->x + x * outline->xScale, outline->y + y * outline->yScale);
        }, nullptr, nullptr);

        hb_draw_funcs_set_line_to_func(drawFuncs, [](hb_draw_funcs_t*, void* data, hb_draw_state_t*, float x, float y, void*) {
            auto outline = static_cast<GlyphOutline*>(data);
            plutovg_canvas_line_to(outline->canvas, outline->x + x * outline->xScale, outline->y + y * outline->yScale);
        }, nullptr, nullptr);

        hb_draw_funcs_set_quadratic_to_func(drawFuncs, [](hb_draw_funcs_t*, void* data, hb_draw_state_t*, float x1, float y1, float x2, float y2, void*) {
            auto outline = static_cast<GlyphOutline*>(data);
            plutovg_canvas_quad_to(outline->canvas, outline->x + x1 * outline->xScale, outline->y + y1 * outline->yScale,
                outline->x + x2 * outline->xScale, outline->y + y2 * outline->yScale);
        }, nullptr, nullptr);

        hb_draw_funcs_set_cubic_to_func(drawFuncs, [](hb_draw_funcs_t*, void* data, hb_draw_state_t*, float x1, float y1, float x2, float y2, float x3, float y3, void*) {
            auto outline = static_cast<GlyphOutline*>(data);
            plutovg_canvas_cubic_to(outline->canvas, outline->x + x1 * outline->xScale, outline->y + y1 * outline->yScale,
                outline->x + x2 * outline->xScale, outline->y + y2 * outline->yScale, outline->x + x3 * outline->xScale, outline->y + y3 * outline->yScale);
        }, nullptr, nullptr);

        hb_draw_funcs_set_close_path_func(drawFuncs, [](hb_draw_funcs_t*, void* data, hb_draw_state_t*, void*) {
            auto outline = static_cast<GlyphOutline*>(data);
            plutovg_canvas_close_path(outline->canvas);
        }, nullptr, nullptr);

        hb_draw_funcs_make_immutable(drawFuncs);
        return drawFuncs;
    }();

    return drawFuncs;
}

static bool isColorGlyph(hb_font_t* font, hb_codepoint_t glyph)
{
    auto face = hb_font_get_face(font);
    if(hb_ot_color_glyph_has_paint(face, glyph) || hb_ot_color_glyph_get_layers(face, glyph, 0, nullptr, nullptr) > 0)
        return true;
    return hb_ot_color_has_png(face) && hasPngImage(font, glyph);
}

struct GlyphPainter {
    plutovg_canvas_t* canvas;
    plutovg_color_t foreground;
};

static plutovg_color_t paintColor(const GlyphPainter* painter, bool isForeground, hb_color_t color)
{
    auto alpha = hb_color_get_alpha(color) / 255.f;
    if(isForeground)
        return {painter->foreground.r, painter->foreground.g, painter->foreground.b, painter->foreground.a * alpha};
    return {hb_color_get_red(color) / 255.f, hb_color_get_green(color) / 255.f, hb_color_get_blue(color) / 255.f, alpha};
}

// the stops of a color line with their offsets mapped to 0...1, `start` and `end` receive the original range
static std::vector<plutovg_gradient_stop_t> colorStops(const GlyphPainter* painter, hb_color_line_t* colorLine, float& start, float& end)
{
    auto count = hb_color_line_get_color_stops(colorLine, 0, nullptr, nullptr);
    std::vector<hb_color_stop_t> colorStops(count);
    hb_color_line_get_color_stops(colorLine, 0, &count, colorStops.data());
    std::stable_sort(colorStops.begin(), colorStops.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
    start = colorStops.empty() ? 0.f : colorStops.front().offset;
    end = colorStops.empty() ? 1.f : colorStops.back().offset;
    auto range = end - start;
    std::vector<plutovg_gradient_stop_t> stops;
    for(const auto& colorStop : colorStops) {
        auto offset = range > 0.f ? (colorStop.offset - start) / range : 0.f;
        stops.push_back({offset, paintColor(painter, colorStop.is_foreground, colorStop.color)});
    }

    return stops;
}

static plutovg_spread_method_t spreadMethod(hb_color_line_t* colorLine)
{
    switch(hb_color_line_get_extend(colorLine)) {
    case HB_PAINT_EXTEND_REPEAT:
        return PLUTOVG_SPREAD_METHOD_REPEAT;
    case HB_PAINT_EXTEND_REFLECT:
        return PLUTOVG_SPREAD_METHOD_REFLECT;
    default:
        return PLUTOVG_SPREAD_METHOD_PAD;
    }
}

static void paintEverything(plutovg_canvas_t* canvas)
{
    plutovg_canvas_fill_rect(canvas, -32768.f, -32768.f, 65536.f, 65536.f);
}

// color glyphs (COLR, sbix, CBDT) are painted by HarfBuzz; plutovg has no sweep gradients and no group
// compositing, so sweeps are filled with their middle color and groups are drawn over the backdrop
static hb_paint_funcs_t* glyphPaintFuncs()
{
    static hb_paint_funcs_t* paintFuncs = [] {
        auto paintFuncs = hb_paint_funcs_create();
        hb_paint_funcs_set_push_transform_func(paintFuncs, [](hb_paint_funcs_t*, void* data, float xx, float yx, float xy, float yy, float dx, float dy, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            plutovg_matrix_t matrix = {xx, yx, xy, yy, dx, dy};
            plutovg_canvas_save(painter->canvas);
            plutovg_canvas_transform(painter->canvas, &matrix);
        }, nullptr, nullptr);

        hb_paint_funcs_set_pop_transform_func(paintFuncs, [](hb_paint_funcs_t*, void* data, void*) {
            plutovg_canvas_restore(static_cast<GlyphPainter*>(data)->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_push_clip_glyph_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_codepoint_t glyph, hb_font_t* font, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            plutovg_canvas_save(painter->canvas);
            plutovg_canvas_new_path(painter->canvas);
            GlyphOutline outline = {painter->canvas, 0.f, 0.f, 1.f, 1.f};
            hb_font_draw_glyph(font, glyph, glyphDrawFuncs(), &outline);
            plutovg_canvas_clip(painter->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_push_clip_rectangle_func(paintFuncs, [](hb_paint_funcs_t*, void* data, float xmin, float ymin, float xmax, float ymax, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            plutovg_canvas_save(painter->canvas);
            plutovg_canvas_clip_rect(painter->canvas, xmin, ymin, xmax - xmin, ymax - ymin);
        }, nullptr, nullptr);

        hb_paint_funcs_set_pop_clip_func(paintFuncs, [](hb_paint_funcs_t*, void* data, void*) {
            plutovg_canvas_restore(static_cast<GlyphPainter*>(data)->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_color_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_bool_t isForeground, hb_color_t color, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            auto paint = paintColor(painter, isForeground, color);
            plutovg_canvas_set_color(painter->canvas, &paint);
            paintEverything(painter->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_image_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_blob_t* image, unsigned int width, unsigned int height, hb_tag_t format, float, hb_glyph_extents_t* extents, void*) -> hb_bool_t {
            auto painter = static_cast<GlyphPainter*>(data);
            if(format != HB_PAINT_IMAGE_FORMAT_PNG || extents == nullptr || width == 0 || height == 0)
                return false;
            unsigned int length = 0;
            auto imageData = hb_blob_get_data(image, &length);
            auto surface = plutovg_surface_load_from_image_data(imageData, length);
            if(surface == nullptr)
                return false;
            plutovg_matrix_t matrix;
            plutovg_matrix_init_translate(&matrix, extents->x_bearing, extents->y_bearing);
            plutovg_matrix_scale(&matrix, float(extents->width) / plutovg_surface_get_width(surface), float(extents->height) / plutovg_surface_get_height(surface));
            plutovg_canvas_set_texture(painter->canvas, surface, PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &matrix);
            plutovg_canvas_fill_rect(painter->canvas, extents->x_bearing, extents->y_bearing + extents->height, extents->width, -extents->height);
            plutovg_surface_destroy(surface);
            return true;
        }, nullptr, nullptr);

        hb_paint_funcs_set_linear_gradient_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_color_line_t* colorLine, float x0, float y0, float x1, float y1, float x2, float y2, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            // COLRv1 rotates the gradient line by the third point, p1 moves onto the normal of p0 p2
            auto dx = x2 - x0, dy = y2 - y0;
            auto length = dx * dx + dy * dy;
            if(length > 1e-6f) {
                auto k = ((x1 - x0) * dx + (y1 - y0) * dy) / length;
                x1 -= k * dx;
                y1 -= k * dy;
            }

            float start, end;
            auto stops = colorStops(painter, colorLine, start, end);
            auto sx = x0 + start * (x1 - x0), sy = y0 + start * (y1 - y0);
            auto ex = x0 + end * (x1 - x0), ey = y0 + end * (y1 - y0);
            plutovg_canvas_set_linear_gradient(painter->canvas, sx, sy, ex, ey, spreadMethod(colorLine), stops.data(), stops.size(), nullptr);
            paintEverything(painter->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_radial_gradient_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_color_line_t* colorLine, float x0, float y0, float r0, float x1, float y1, float r1, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            float start, end;
            auto stops = colorStops(painter, colorLine, start, end);
            auto fx = x0 + start * (x1 - x0), fy = y0 + start * (y1 - y0), fr = r0 + start * (r1 - r0);
            auto cx = x0 + end * (x1 - x0), cy = y0 + end * (y1 - y0), cr = r0 + end * (r1 - r0);
            plutovg_canvas_set_radial_gradient(painter->canvas, cx, cy, std::max(cr, 0.f), fx, fy, std::max(fr, 0.f), spreadMethod(colorLine), stops.data(), stops.size(), nullptr);
            paintEverything(painter->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_set_sweep_gradient_func(paintFuncs, [](hb_paint_funcs_t*, void* data, hb_color_line_t* colorLine, float, float, float, float, void*) {
            auto painter = static_cast<GlyphPainter*>(data);
            float start, end;
            auto stops = colorStops(painter, colorLine, start, end);
            if(stops.empty())
                return;
            plutovg_canvas_set_color(painter->canvas, &stops[stops.size() / 2].color);
            paintEverything(painter->canvas);
        }, nullptr, nullptr);

        hb_paint_funcs_make_immutable(paintFuncs);
        return paintFuncs;
    }();

    return paintFuncs;
}

static void paintColorGlyph(plutovg_canvas_t* canvas, hb_font_t* font, hb_codepoint_t glyph, float x, float y, float scale, const plutovg_color_t& foreground)
{
    plutovg_canvas_save(canvas);
    plutovg_matrix_t matrix = {scale, 0.f, 0.f, -scale, x, y};
    plutovg_canvas_transform(canvas, &matrix);
    GlyphPainter painter = {canvas, foreground};
    auto color = HB_COLOR(uint8_t(foreground.b * 255), uint8_t(foreground.g * 255), uint8_t(foreground.r * 255), uint8_t(foreground.a * 255));
    hb_font_paint_glyph(font, glyph, glyphPaintFuncs(), &painter, 0, color);
    plutovg_canvas_restore(canvas);
}

struct ColorGlyph {
    hb_font_t* font;
    hb_codepoint_t glyph;
    float x;
    float y;
    float scale;
};

static uint32_t toArgb(const plutovg_color_t& color)
{
    auto channel = [](float value) { return uint32_t(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f); };
    return channel(color.a) << 24 | channel(color.r) << 16 | channel(color.g) << 8 | channel(color.b);
}

struct PlatformRun {
    hb_font_t* font;
    std::u32string text;
    float x;
    float y;
    float size;
    int weight;
    bool italic;
};

// draws text of the platform, requested at the pixel size of the canvas so it stays sharp
static void paintPlatformRun(plutovg_canvas_t* canvas, const PlatformRun& run, const plutovg_color_t& foreground)
{
    plutovg_matrix_t matrix;
    plutovg_canvas_get_matrix(canvas, &matrix);
    auto pixelScale = std::sqrt(std::abs(matrix.a * matrix.d - matrix.b * matrix.c));
    if(pixelScale <= 0.f)
        return;
    int index = 0;
    auto data = fontData(run.font, index);
    auto advance = 0.f;
    lunasvg_text_image_t image = {};
    if(!fontFaceCache()->getTextImage(data, index, run.text, run.size * pixelScale, run.weight, run.italic, toArgb(foreground), &advance, &image) || image.data == nullptr)
        return;
    auto surface = plutovg_surface_create_for_data(image.data, image.width, image.height, image.stride);
    auto x = run.x + image.left / pixelScale;
    auto y = run.y - image.top / pixelScale;
    plutovg_matrix_t textureMatrix;
    plutovg_matrix_init_translate(&textureMatrix, x, y);
    plutovg_matrix_scale(&textureMatrix, 1.f / pixelScale, 1.f / pixelScale);
    plutovg_canvas_save(canvas);
    plutovg_canvas_set_texture(canvas, surface, PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &textureMatrix);
    plutovg_canvas_fill_rect(canvas, x, y, image.width / pixelScale, image.height / pixelScale);
    plutovg_canvas_restore(canvas);
    plutovg_surface_destroy(surface);
    free(image.data);
}

// adds the outlines of the glyphs to the path; color glyphs and platform runs go to `colorGlyphs` and
// `platformRuns` if given, with a font reference
static void addText(plutovg_canvas_t* canvas, const std::u32string_view& text, const Font& font, const Point& origin, bool rightToLeft,
    const std::u32string_view& context, std::vector<ColorGlyph>* colorGlyphs, std::vector<PlatformRun>* platformRuns)
{
    auto glyphFunc = [&](hb_font_t* hbFont, uint32_t glyph, float x, float y, float scale) {
        if(colorGlyphs && isColorGlyph(hbFont, glyph)) {
            colorGlyphs->push_back({hb_font_reference(hbFont), glyph, origin.x + x, origin.y - y, scale});
            return;
        }

        GlyphOutline outline = {canvas, origin.x + x, origin.y - y, scale, -scale};
        hb_font_draw_glyph(hbFont, glyph, glyphDrawFuncs(), &outline);
    };

    auto runFunc = [&](hb_font_t* hbFont, const std::u32string_view& runText, float x) {
        if(platformRuns) {
            platformRuns->push_back({hb_font_reference(hbFont), std::u32string(runText), origin.x + x, origin.y, font.size(), font.weight(), font.italic()});
        }
    };

    shapeText(font, text, rightToLeft, context, glyphFunc, runFunc);
}

#endif // LUNASVG_ENABLE_TEXT_SHAPING

Font::Font(const FontFace& face, float size, int weight, bool italic, const std::string& language, const std::string& family, [[maybe_unused]] const std::vector<FontFace>& fallbackFaces)
    : m_face(face), m_size(size), m_weight(weight), m_italic(italic), m_language(language), m_family(family)
{
    if(m_size > 0.f && !m_face.isNull()) {
        plutovg_font_face_get_metrics(m_face.get(), m_size, &m_ascent, &m_descent, &m_lineGap, nullptr);
#ifdef LUNASVG_ENABLE_TEXT_SHAPING
        std::vector<plutovg_font_face_t*> faces = {m_face.get()};
        for(const auto& fallbackFace : fallbackFaces) {
            if(!fallbackFace.isNull() && std::find(faces.begin(), faces.end(), fallbackFace.get()) == faces.end()) {
                faces.push_back(fallbackFace.get());
            }
        }

        for(auto face : faces) {
            m_hbFonts.push_back(createHbFont(face, m_weight, m_italic));
        }
#endif
    }
}

float Font::xHeight() const
{
#ifdef LUNASVG_ENABLE_TEXT_SHAPING
    // like browsers: the x-height of the OS/2 table, the height of the "x" glyph, or half an em
    if(!m_hbFonts.empty()) {
        auto hbFont = m_hbFonts.front().get();
        auto scale = m_size / hb_face_get_upem(hb_font_get_face(hbFont));
        hb_position_t xHeight = 0;
        if(hb_ot_metrics_get_position(hbFont, HB_OT_METRICS_TAG_X_HEIGHT, &xHeight) && xHeight > 0)
            return xHeight * scale;
        hb_codepoint_t glyph = 0;
        hb_glyph_extents_t extents;
        if(hb_font_get_nominal_glyph(hbFont, 'x', &glyph) && hb_font_get_glyph_extents(hbFont, glyph, &extents) && extents.y_bearing > 0)
            return extents.y_bearing * scale;
        return m_size / 2.f;
    }
#endif
    plutovg_rect_t extents = {0};
    if(m_size > 0.f && !m_face.isNull())
        plutovg_font_face_get_glyph_metrics(m_face.get(), m_size, 'x', nullptr, nullptr, &extents);
    return extents.h;
}

float Font::measureText(const std::u32string_view& text, [[maybe_unused]] bool rightToLeft, [[maybe_unused]] const std::u32string_view& context) const
{
#ifdef LUNASVG_ENABLE_TEXT_SHAPING
    if(!m_hbFonts.empty())
        return shapeText(*this, text, rightToLeft, context, [](hb_font_t*, uint32_t, float, float, float) {}, [](hb_font_t*, const std::u32string_view&, float) {});
#endif
    if(m_size > 0.f && !m_face.isNull())
        return plutovg_font_face_text_extents(m_face.get(), m_size, text.data(), text.length(), PLUTOVG_TEXT_ENCODING_UTF32, nullptr);
    return 0;
}

std::shared_ptr<Canvas> Canvas::create(const Bitmap& bitmap)
{
    return std::shared_ptr<Canvas>(new Canvas(bitmap));
}

std::shared_ptr<Canvas> Canvas::create(float x, float y, float width, float height)
{
    constexpr int kMaxSize = 1 << 15;
    if(width <= 0 || height <= 0 || width >= kMaxSize || height >= kMaxSize)
        return std::shared_ptr<Canvas>(new Canvas(0, 0, 1, 1));
    auto l = static_cast<int>(std::floor(x));
    auto t = static_cast<int>(std::floor(y));
    auto r = static_cast<int>(std::ceil(x + width));
    auto b = static_cast<int>(std::ceil(y + height));
    return std::shared_ptr<Canvas>(new Canvas(l, t, r - l, b - t));
}

std::shared_ptr<Canvas> Canvas::create(const Rect& extents)
{
    return create(extents.x, extents.y, extents.w, extents.h);
}

void Canvas::setColor(const Color& color)
{
    setColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

void Canvas::setColor(float r, float g, float b, float a)
{
    plutovg_canvas_set_rgba(m_canvas, r, g, b, a);
}

void Canvas::setLinearGradient(float x1, float y1, float x2, float y2, SpreadMethod spread, const GradientStops& stops, const Transform& transform)
{
    plutovg_canvas_set_linear_gradient(m_canvas, x1, y1, x2, y2, static_cast<plutovg_spread_method_t>(spread), stops.data(), stops.size(), &transform.matrix());
}

void Canvas::setRadialGradient(float cx, float cy, float r, float fx, float fy, SpreadMethod spread, const GradientStops& stops, const Transform& transform)
{
    plutovg_canvas_set_radial_gradient(m_canvas, cx, cy, r, fx, fy, 0.f, static_cast<plutovg_spread_method_t>(spread), stops.data(), stops.size(), &transform.matrix());
}

void Canvas::setTexture(const Canvas& source, TextureType type, float opacity, const Transform& transform)
{
    plutovg_canvas_set_texture(m_canvas, source.surface(), static_cast<plutovg_texture_type_t>(type), opacity, &transform.matrix());
}

void Canvas::fillPath(const Path& path, FillRule fillRule, const Transform& transform)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_fill_rule(m_canvas, static_cast<plutovg_fill_rule_t>(fillRule));
    plutovg_canvas_set_operator(m_canvas, PLUTOVG_OPERATOR_SRC_OVER);
    plutovg_canvas_fill_path(m_canvas, path.data());
}

void Canvas::strokePath(const Path& path, const StrokeData& strokeData, const Transform& transform)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_line_width(m_canvas, strokeData.lineWidth());
    plutovg_canvas_set_miter_limit(m_canvas, strokeData.miterLimit());
    plutovg_canvas_set_line_cap(m_canvas, static_cast<plutovg_line_cap_t>(strokeData.lineCap()));
    plutovg_canvas_set_line_join(m_canvas, static_cast<plutovg_line_join_t>(strokeData.lineJoin()));
    plutovg_canvas_set_dash_offset(m_canvas, strokeData.dashOffset());
    plutovg_canvas_set_dash_array(m_canvas, strokeData.dashArray().data(), strokeData.dashArray().size());
    plutovg_canvas_set_operator(m_canvas, PLUTOVG_OPERATOR_SRC_OVER);
    plutovg_canvas_stroke_path(m_canvas, path.data());
}

void Canvas::fillText(const std::u32string_view& text, const Font& font, const Point& origin, const Transform& transform, [[maybe_unused]] bool rightToLeft, [[maybe_unused]] const std::u32string_view& context)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_fill_rule(m_canvas, PLUTOVG_FILL_RULE_NON_ZERO);
    plutovg_canvas_set_operator(m_canvas, PLUTOVG_OPERATOR_SRC_OVER);
#ifdef LUNASVG_ENABLE_TEXT_SHAPING
    if(!font.hbFonts().empty()) {
        plutovg_canvas_new_path(m_canvas);
        std::vector<ColorGlyph> colorGlyphs;
        std::vector<PlatformRun> platformRuns;
        addText(m_canvas, text, font, origin, rightToLeft, context, &colorGlyphs, &platformRuns);
        plutovg_canvas_fill(m_canvas);
        plutovg_color_t foreground = {0.f, 0.f, 0.f, 1.f};
        plutovg_canvas_get_paint(m_canvas, &foreground);
        for(const auto& colorGlyph : colorGlyphs) {
            paintColorGlyph(m_canvas, colorGlyph.font, colorGlyph.glyph, colorGlyph.x, colorGlyph.y, colorGlyph.scale, foreground);
            hb_font_destroy(colorGlyph.font);
        }

        for(const auto& platformRun : platformRuns) {
            paintPlatformRun(m_canvas, platformRun, foreground);
            hb_font_destroy(platformRun.font);
        }
        return;
    }
#endif
    plutovg_canvas_set_font(m_canvas, font.face().get(), font.size());
    plutovg_canvas_fill_text(m_canvas, text.data(), text.length(), PLUTOVG_TEXT_ENCODING_UTF32, origin.x, origin.y);
}

void Canvas::strokeText(const std::u32string_view& text, float strokeWidth, const Font& font, const Point& origin, const Transform& transform, [[maybe_unused]] bool rightToLeft, [[maybe_unused]] const std::u32string_view& context)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_line_width(m_canvas, strokeWidth);
    plutovg_canvas_set_miter_limit(m_canvas, 4.f);
    plutovg_canvas_set_line_cap(m_canvas, PLUTOVG_LINE_CAP_BUTT);
    plutovg_canvas_set_line_join(m_canvas, PLUTOVG_LINE_JOIN_MITER);
    plutovg_canvas_set_dash_offset(m_canvas, 0.f);
    plutovg_canvas_set_dash_array(m_canvas, nullptr, 0);
    plutovg_canvas_set_operator(m_canvas, PLUTOVG_OPERATOR_SRC_OVER);
#ifdef LUNASVG_ENABLE_TEXT_SHAPING
    if(!font.hbFonts().empty()) {
        plutovg_canvas_new_path(m_canvas);
        addText(m_canvas, text, font, origin, rightToLeft, context, nullptr, nullptr);
        plutovg_canvas_stroke(m_canvas);
        return;
    }
#endif
    plutovg_canvas_set_font(m_canvas, font.face().get(), font.size());
    plutovg_canvas_stroke_text(m_canvas, text.data(), text.length(), PLUTOVG_TEXT_ENCODING_UTF32, origin.x, origin.y);
}

void Canvas::clipPath(const Path& path, FillRule clipRule, const Transform& transform)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_fill_rule(m_canvas, static_cast<plutovg_fill_rule_t>(clipRule));
    plutovg_canvas_clip_path(m_canvas, path.data());
}

void Canvas::clipRect(const Rect& rect, FillRule clipRule, const Transform& transform)
{
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_set_fill_rule(m_canvas, static_cast<plutovg_fill_rule_t>(clipRule));
    plutovg_canvas_clip_rect(m_canvas, rect.x, rect.y, rect.w, rect.h);
}

void Canvas::drawImage(const Bitmap& image, const Rect& dstRect, const Rect& srcRect, const Transform& transform)
{
    auto xScale = dstRect.w / srcRect.w;
    auto yScale = dstRect.h / srcRect.h;
    plutovg_matrix_t matrix = { xScale, 0, 0, yScale, -srcRect.x * xScale, -srcRect.y * yScale };
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_transform(m_canvas, &transform.matrix());
    plutovg_canvas_translate(m_canvas, dstRect.x, dstRect.y);
    plutovg_canvas_set_fill_rule(m_canvas, PLUTOVG_FILL_RULE_NON_ZERO);
    plutovg_canvas_set_operator(m_canvas, PLUTOVG_OPERATOR_SRC_OVER);
    plutovg_canvas_set_texture(m_canvas, image.surface(), PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &matrix);
    plutovg_canvas_fill_rect(m_canvas, 0, 0, dstRect.w, dstRect.h);
}

void Canvas::blendCanvas(const Canvas& canvas, BlendMode blendMode, float opacity)
{
    plutovg_matrix_t matrix = { 1, 0, 0, 1, static_cast<float>(canvas.x()), static_cast<float>(canvas.y()) };
    plutovg_canvas_set_matrix(m_canvas, &m_translation);
    plutovg_canvas_set_operator(m_canvas, static_cast<plutovg_operator_t>(blendMode));
    plutovg_canvas_set_texture(m_canvas, canvas.surface(), PLUTOVG_TEXTURE_TYPE_PLAIN, opacity, &matrix);
    plutovg_canvas_paint(m_canvas);
}

void Canvas::save()
{
    plutovg_canvas_save(m_canvas);
}

void Canvas::restore()
{
    plutovg_canvas_restore(m_canvas);
}

int Canvas::width() const
{
    return plutovg_surface_get_width(m_surface);
}

int Canvas::height() const
{
    return plutovg_surface_get_height(m_surface);
}

void Canvas::convertToLuminanceMask()
{
    auto width = plutovg_surface_get_width(m_surface);
    auto height = plutovg_surface_get_height(m_surface);
    auto stride = plutovg_surface_get_stride(m_surface);
    auto data = plutovg_surface_get_data(m_surface);
    for(int y = 0; y < height; y++) {
        auto pixels = reinterpret_cast<uint32_t*>(data + stride * y);
        for(int x = 0; x < width; x++) {
            auto pixel = pixels[x];
            auto a = (pixel >> 24) & 0xFF;
            auto r = (pixel >> 16) & 0xFF;
            auto g = (pixel >> 8) & 0xFF;
            auto b = (pixel >> 0) & 0xFF;
            if(a) {
                r = (r * 255) / a;
                g = (g * 255) / a;
                b = (b * 255) / a;
            }

            auto l = (r * 0.2125 + g * 0.7154 + b * 0.0721);
            pixels[x] = static_cast<uint32_t>(l * (a / 255.0)) << 24;
        }
    }
}

Canvas::~Canvas()
{
    plutovg_canvas_destroy(m_canvas);
    plutovg_surface_destroy(m_surface);
}

Canvas::Canvas(const Bitmap& bitmap)
    : m_surface(plutovg_surface_reference(bitmap.surface()))
    , m_canvas(plutovg_canvas_create(m_surface))
    , m_translation({1, 0, 0, 1, 0, 0})
    , m_x(0), m_y(0)
{
}

Canvas::Canvas(int x, int y, int width, int height)
    : m_surface(plutovg_surface_create(width, height))
    , m_canvas(plutovg_canvas_create(m_surface))
    , m_translation({1, 0, 0, 1, -static_cast<float>(x), -static_cast<float>(y)})
    , m_x(x), m_y(y)
{
}

} // namespace lunasvg
