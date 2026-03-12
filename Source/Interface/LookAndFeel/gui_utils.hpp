
// Created: 2025-02-27 04:51:34

#pragma once

#include "nanovg/nanovg.h"

#include "Framework/stl_utils.hpp"

extern "C"
{
  typedef struct NVGcolor NVGcolor;
}

namespace utils
{
  struct Allocator;
  class string;
}

namespace Interface
{
  template<typename T>
  struct Area
  {
    // thanks to common initial sequence rules
    // we can do the following

    union
    {
      T x{};
      T w;
      T top;
      T left;
      T min;
      T begin;
    };

    union
    {
      T y{};
      T h;
      T bottom;
      T right;
      T max;
      T end;
    };

    friend constexpr bool operator==(Area lhs, Area rhs) 
    { return lhs.x == rhs.x && lhs.y == rhs.y; };
  };

  template<typename First, typename ... Rest> requires (utils::is_same_v<First, Rest> && ...)
  Area(First, Rest...) -> Area<First>;

  template<typename T>
  using Range = Area<T>;

  template<usize I, typename T> 
  T get(const Area<T> &p)
  {
    if constexpr (I == 0)
      return p.x;
    else
      return p.y;
  }
}

namespace std
{
  template<typename T>
  struct tuple_size<Interface::Area<T>>
  { static constexpr auto value = sizeof(Interface::Area<T>) / sizeof(T); };

  template<usize I, typename T>
  struct tuple_element<I, Interface::Area<T>>
  {
    using type = T;
    static_assert(I < 2, "too many structured bindings for Area");
  };
}

namespace Interface
{
  enum class Placement : u8
  {
    // centered on x and y by default
    centered = 0,

    left = 1 << 0,
    top = 1 << 1,
    right = 1 << 2,
    bottom = 1 << 3,

    justifyX = left | right,
    justifyY = top | bottom,

    // leaves user to do manual positioning
    custom = 1 << 4,
  };

  COMPLEX_DEFINE_ENUM_OPERATION(Placement, &, u8)
  COMPLEX_DEFINE_ENUM_OPERATION(Placement, |, u8)
  COMPLEX_DEFINE_ENUM_OPERATION(Placement, <<, u8)

  template<typename T>
  struct Point
  {
    friend constexpr bool operator==(Point lhs, Point rhs) = default;

    constexpr Point withX(T newX) const { return { newX, y }; }
    constexpr Point withY(T newY) const { return { x, newY }; }
    constexpr Point translated(T deltaX, T deltaY) const { return { x + deltaX, y + deltaY }; }
    constexpr Point transposed() { return { y, x }; }

    constexpr Point &translate(T deltaX, T deltaY) { return *this = translated(deltaX, deltaY); }
    constexpr Point &transpose() { return *this = transposed(); }

    constexpr Point operator+(Point other) const { return { x + other.x, y + other.y }; }
    constexpr Point &operator+=(Point other) { x += other.x; y += other.y; return *this; }
    constexpr Point operator-(Point other) const { return { x - other.x, y - other.y }; }
    constexpr Point &operator-=(Point other) { x -= other.x; y -= other.y; return *this; }
    constexpr Point operator-() const { return { -x, -y }; }
    constexpr Point operator*(T multiplier) const { return { x * multiplier, y * multiplier }; }
    constexpr Point &operator*=(T multiplier) { x *= multiplier; y *= multiplier; return *this; }
    constexpr Point operator/(T multiplier) const { return { x / multiplier, y / multiplier }; }
    constexpr Point &operator/=(T multiplier) { x /= multiplier; y /= multiplier; return *this; }

    constexpr Point<int> toInt() const { return Point<int>{ (int)x, (int)y }; }
    constexpr Point<float> toFloat() const { return Point<float>{ (float)x, (float)y }; }
    constexpr Point<double> toDouble() const { return Point<double>{ (double)x, (double)y }; }

    T x{}, y{};
  };

  template<typename T>
  struct Rectangle
  {
    constexpr Rectangle() = default;
    constexpr Rectangle(Point<T> position, T width, T height) :
      x(position.x), y(position.y), w(width), h(height) { }
    constexpr Rectangle(T initialX, T initialY, T width, T height) :
      x(initialX), y(initialY), w(width), h(height) { }
    constexpr Rectangle(T width, T height) : w(width), h(height) { }
    constexpr Rectangle(Point<T> corner1, Point<T> corner2) :
      x{ utils::min(corner1.x, corner2.x) }, y{ utils::min(corner1.y, corner2.y) },
      w{ utils::abs(corner1.x - corner2.x) }, h{ utils::abs(corner1.y - corner2.y) } { }

    friend constexpr bool operator==(Rectangle lhs, Rectangle rhs) = default;

    static constexpr Rectangle 
    leftTopRightBottom(T left, T top, T right, T bottom)
    {
      return { left, top, right - left, bottom - top };
    }

    constexpr T getRight() const { return x + w; }
    constexpr T getBottom() const { return y + h; }
    constexpr T getCentreX() const { return x + w / (T)2; }
    constexpr T getCentreY() const { return y + h / (T)2; }
    constexpr Point<T> getCentre() const { return { x + w / (T)2, y + h / (T)2 }; }
    constexpr Point<T> getPosition() const { return { x, y }; }
    constexpr Point<T> getTopLeft() const { return getPosition(); }
    constexpr Point<T> getTopRight() const { return { x + w, y }; }
    constexpr Point<T> getBottomLeft() const { return { x, y + h }; }
    constexpr Point<T> getBottomRight() const { return { x + w, y + h }; }

    constexpr Rectangle withX(T newX) const { return { newX, y, w, h }; }
    constexpr Rectangle withY(T newY) const { return { x, newY, w, h }; }
    constexpr Rectangle withWidth(T newWidth) const { return { x, y, utils::max(T(), newWidth), h }; }
    constexpr Rectangle withHeight(T newHeight) const { return { x, y, w, utils::max(T(), newHeight) }; }

    constexpr Rectangle withPosition(T newX, T newY) const { return { newX, newY, w, h }; }
    constexpr Rectangle withPosition(Point<T> newPosition) const { return { newPosition.x, newPosition.y, w, h }; }
    constexpr Rectangle withCentre(T newCentreX, T newCentreY) const { return { newCentreX - w / (T)2, newCentreY - h / (T)2, w, h }; }
    constexpr Rectangle withSize(T newWidth, T newHeight) const { return { x, y, utils::max(T(), newWidth), utils::max(T(), newHeight) }; }
    constexpr Rectangle withZeroOrigin() const { return { w, h }; }
    constexpr Rectangle withShift(T deltaX, T deltaY) const { return { x + deltaX, y + deltaY, w, h }; }

    constexpr Rectangle withLeft(T newLeft) const { return { newLeft, y, utils::max(T(), x + w - newLeft), h }; }
    constexpr Rectangle withTop(T newTop) const { return { x, newTop, w, utils::max(T(), y + h - newTop) }; }
    constexpr Rectangle withRight(T newRight) const { return { utils::min(x, newRight), y, utils::max(T(), newRight - x), h }; }
    constexpr Rectangle withBottom(T newBottom) const { return { x, utils::min(y, newBottom), w, utils::max(T(), newBottom - y) }; }

    constexpr Rectangle withExpandedLeft(T delta) const { return withLeft(x - delta); }
    constexpr Rectangle withExpandedTop(T delta) const { return withTop(y - delta); }
    constexpr Rectangle withExpandedRight(T delta) const { return withWidth(w + delta); }
    constexpr Rectangle withExpandedBottom(T delta) const { return withHeight(h + delta); }

    constexpr Rectangle withTrimmedLeft(T delta) const { return withLeft(x + delta); }
    constexpr Rectangle withTrimmedTop(T delta) const { return withTop(y + delta); }
    constexpr Rectangle withTrimmedRight(T delta) const { return withWidth(w - delta); }
    constexpr Rectangle withTrimmedBottom(T delta) const { return withHeight(h - delta); }



    constexpr Rectangle &setPosition(T newX, T newY) { return *this = withPosition(newX, newY); }
    constexpr Rectangle &setPosition(Point<T> newPosition) { return *this = withPosition(newPosition.x, newPosition.y); }
    constexpr Rectangle &setSize(T newWidth, T newHeight) { return *this = withSize(newWidth, newHeight); }
    constexpr Rectangle &setCentre(T newCentreX, T newCentreY) { return *this = withCentre(newCentreX, newCentreY); }

    constexpr Rectangle &setLeft(T newLeft) { return *this = withLeft(newLeft); }
    constexpr Rectangle &setTop(T newTop) { return *this = withTop(newTop); }
    constexpr Rectangle &setRight(T newRight) { return *this = withRight(newRight); }
    constexpr Rectangle &setBottom(T newBottom) { return *this = withBottom(newBottom); }
    constexpr Rectangle &shift(T deltaX, T deltaY) { return *this = withShift(deltaX, deltaY); }

    constexpr Rectangle &expandLeft(T delta) { return *this = withTrimmedLeft(delta); }
    constexpr Rectangle &expandTop(T delta) { return *this = withTrimmedTop(delta); }
    constexpr Rectangle &expandRight(T delta) { return *this = withTrimmedRight(delta); }
    constexpr Rectangle &expandBottom(T delta) { return *this = withTrimmedBottom(delta); }
    
    constexpr Rectangle &trimLeft(T delta) { return *this = withTrimmedLeft(delta); }
    constexpr Rectangle &trimTop(T delta) { return *this = withTrimmedTop(delta); }
    constexpr Rectangle &trimRight(T delta) { return *this = withTrimmedRight(delta); }
    constexpr Rectangle &trimBottom(T delta) { return *this = withTrimmedBottom(delta); }

    constexpr Rectangle operator+(Point<T> deltaPosition) const { return { x + deltaPosition.x, y + deltaPosition.y, w, h }; }
    constexpr Rectangle operator-(Point<T> deltaPosition) const { return { x - deltaPosition.x, y - deltaPosition.y, w, h }; }

    constexpr Rectangle &operator+=(Point<T> deltaPosition) { x += deltaPosition.x; y += deltaPosition.y; return *this; }
    constexpr Rectangle &operator-=(Point<T> deltaPosition) { x -= deltaPosition.x; y -= deltaPosition.y; return *this; }

    constexpr Rectangle expanded(Rectangle<T> delta) const
    {
      return { x - delta.x, y - delta.y, utils::max(T(), w + delta.x + delta.w),
        utils::max(T(), h + delta.y + delta.h) };
    }
    constexpr Rectangle expanded(T deltaX, T deltaY) const
    {
      return { x - deltaX, y - deltaY, utils::max(T(), w + deltaX * 2), 
        utils::max(T(), h + deltaY * 2) };
    }
    constexpr Rectangle expanded(T delta) const { return expanded(delta, delta); }
    constexpr Rectangle trimmed(Rectangle<T> delta) const { return expanded({ -delta.x, -delta.y, -delta.w, -delta.h }); }
    constexpr Rectangle trimmed(T deltaX, T deltaY) const { return expanded(-deltaX, -deltaY); }
    constexpr Rectangle trimmed(T delta) const { return expanded(-delta); }
    constexpr Rectangle transposed() { return { y, x, h, w }; }

    constexpr Rectangle &expand(Rectangle<T> delta) { return *this = expanded(delta); }
    constexpr Rectangle &expand(T deltaX, T deltaY) { return *this = expanded(deltaX, deltaY); }
    constexpr Rectangle &expand(T delta) { *this = expanded(delta); }
    constexpr Rectangle &trim(Rectangle<T> delta) { return *this = trimmed(delta); }
    constexpr Rectangle &trim(T deltaX, T deltaY) { return *this = trimmed(deltaX, deltaY); }
    constexpr Rectangle &trim(T delta) { return *this = trimmed(delta); }
    constexpr Rectangle &transpose() { return *this = transposed(); }



    constexpr bool isEmpty() const { return w <= T() || h <= T(); }
    constexpr bool contains(T xCoord, T yCoord) const
    {
      return xCoord >= x && yCoord >= y && xCoord < x + w && yCoord < y + h;
    }
    constexpr bool contains(Point<T> point) const { return contains(point.x, point.y); }
    constexpr bool contains(Rectangle other) const
    {
      return x <= other.x && y <= other.y && x + w >= other.x + other.w && y + h >= other.y + other.h;
    }

    constexpr bool intersects(Rectangle other) const
    {
      return x + w > other.x
        && y + h > other.y
        && x < other.x + other.w
        && y < other.y + other.h
        && w > T() && h > T()
        && other.w > T() && other.h > T();
    }

    constexpr Rectangle getIntersection(Rectangle other) const
    {
      auto nx = utils::max(x, other.x);
      auto ny = utils::max(y, other.y);
      auto nw = utils::min(x + w, other.x + other.w) - nx;
      auto nh = utils::min(y + h, other.y + other.h) - ny;

      return { nx, ny, nw, nh };
    }

    constexpr bool intersectRectangle(T &otherX, T &otherY, T &otherW, T &otherH) const
    {
      auto maxX = utils::max(otherX, x);
      otherW = utils::min(otherX + otherW, x + w) - maxX;

      auto maxY = utils::max(otherY, y);
      otherH = utils::min(otherY + otherH, y + h) - maxY;

      otherX = maxX; 
      otherY = maxY;

      return otherW > T() && otherH > T();
    }

    constexpr bool intersectRectangle(Rectangle<T> &rectangleToClip) const
    {
      return intersectRectangle(rectangleToClip.x, rectangleToClip.y,
        rectangleToClip.w, rectangleToClip.h);
    }

    constexpr Rectangle getUnion(Rectangle other) const
    {
      if (other.isEmpty())
        return *this;
      if (isEmpty())
        return other;

      auto newX = utils::min(x, other.x);
      auto newY = utils::min(y, other.y);

      return { newX, newY, utils::max(x + w, other.x + other.w) - newX,
        utils::max(y + h, other.y + other.h) - newY };
    }

    constexpr Point<T> getConstrainedPoint(Point<T> point) const
    {
      return { utils::clamp(point.x, x, getRight()),
        utils::clamp(point.y, y, getBottom()) };
    }

    constexpr Rectangle constrainedWithin(Rectangle areaToFitWithin) const
    {
      auto newPos = areaToFitWithin.withSize(areaToFitWithin.w - w,
        areaToFitWithin.h - h).getConstrainedPoint({ x, y });

      return { newPos.x, newPos.y, utils::min(w, areaToFitWithin.w),
        utils::min(h, areaToFitWithin.h) };
    }

    constexpr Rectangle<int> toInt() const 
    { return Rectangle<int>{ (int)x, (int)y, (int)w, (int)h }; }
    constexpr Rectangle<float> toFloat() const 
    { return Rectangle<float>{ (float)x, (float)y, (float)w, (float)h }; }
    constexpr Rectangle<double> toDouble() const 
    { return Rectangle<double>{ (double)x, (double)y, (double)w, (double)h }; }

    T x{}, y{}, w{}, h{};
  };

  struct alignas(u32) Colour
  {
    constexpr Colour() = default;
    explicit constexpr Colour(u32 argb) : a{ u8((argb >> 24) & 0xff) },
      r { u8((argb >> 16) & 0xff) }, g{ u8((argb >> 8) & 0xff) }, b{ u8(argb & 0xff) } { }
    constexpr Colour(int red, int green, int blue, int alpha = 255) :
      a{ (u8)alpha }, r{ (u8)red }, g{ (u8)green }, b{ (u8)blue } {  }
    constexpr Colour(int red, int green, int blue, float alpha) :
      a{ u8(alpha * 255.0f) }, r{ (u8)red }, g{ (u8)green }, b{ (u8)blue } { }
    constexpr Colour(float red, float green, float blue, int alpha = 255) :
      a{ (u8)alpha }, r{ u8(red * 255.0f) }, g{ u8(green * 255.0f) }, b{ u8(blue * 255.0f) } { }
    constexpr Colour(float red, float green, float blue, float alpha) :
      a{ u8(alpha * 255.0f) }, r{ u8(red * 255.0f) }, g{ u8(green * 255.0f) }, b{ u8(blue * 255.0f) } { }

    constexpr Colour(const Colour &) = default;
    constexpr Colour &operator=(const Colour &) = default;


    constexpr u32 getARGB() const { return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | b; }
    constexpr utils::array<float, 4> 
    getNormalisedARGB() const
    { return utils::array{ a / 255.0f, r / 255.0f, g / 255.0f, b / 255.0f }; }
    constexpr utils::array<float, 4>
    getNormalisedRGBA() const
    { return utils::array{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }; }

    constexpr Colour withAlpha(u8 newAlpha) const { return { r, g, b, newAlpha }; }
    constexpr Colour withAlpha(float newAlpha) const { return { r, g, b, u8(newAlpha * 255.0f) }; }
    constexpr Colour withMultipliedAlpha(float alphaMultiplier) const { return { r, g, b, u8((float)a * alphaMultiplier) }; }
    Colour overlaidWith(Colour foregroundColour) const;
    Colour interpolatedWith(Colour other, float t) const;
    Colour withBrightness(float newBrightness) const;
    Colour withRotatedHue(float amountToRotate) const;

    constexpr Colour
    brighter(float amount) const
    {
      COMPLEX_ASSERT(amount >= 0.0f);
      amount = 1.0f / (1.0f + amount);

      return Colour
      { 
        (u8)(255.0f - (amount * (255.0f - r))),
        (u8)(255.0f - (amount * (255.0f - g))),
        (u8)(255.0f - (amount * (255.0f - b))),
        a
      };
    }

    constexpr Colour
    darker(float amount) const
    {
      COMPLEX_ASSERT(amount >= 0.0f);
      amount = 1.0f / (1.0f + amount);

      return Colour
      {
        (u8)(amount * r),
        (u8)(amount * g),
        (u8)(amount * b),
        a
      };
    }

    usize toString(char *buffer, usize bufferSize) const;
    utils::string toString(utils::Allocator allocator) const;
    static Colour fromString(const char *integer, int base = 16);

    operator NVGcolor() const { return nvgRGBA(r, g, b, a); }
    friend constexpr bool operator==(Colour lhs, Colour rhs) = default;

    u8 b{}, g{}, r{}, a{};
  };

  namespace Colours
  {
    inline constexpr Colour transparentBlack{};
    inline constexpr Colour black{ 0xff000000 };
    inline constexpr Colour transparentWhite{ 0x00ffffff };
    inline constexpr Colour white{ 0xffffffff };
  }

  struct ModifierKeys
  {
    i32 flags = 0;

    enum Flags : i32
    {
      noModifiers = 0,
      shiftModifier = 1 << 0,
      // ctrl on macOS is command, its own ctrl is macosCtrlModifier
      ctrlModifier = 1 << 1,
      altModifier = 1 << 2,
      macosCtrlModifier = 1 << 3,
      leftButtonModifier = 1 << 4,
      rightButtonModifier = 1 << 5,
      middleButtonModifier = 1 << 6,
      forwardButtonModifier = 1 << 7,
      backwardButtonModifier = 1 << 8,
      
      // use this to check for popup menus, instead of rightButtonModifier
    #if COMPLEX_MAC
      popupMenuClickModifier = rightButtonModifier | macosCtrlModifier,
    #else
      popupMenuClickModifier = rightButtonModifier,
    #endif

      allKeyboardModifiers = shiftModifier | ctrlModifier | altModifier | macosCtrlModifier,
      allMouseButtonModifiers = leftButtonModifier | rightButtonModifier | middleButtonModifier,
      ctrlAltCommandModifiers = ctrlModifier | altModifier | macosCtrlModifier
    };

    constexpr ModifierKeys() = default;
    constexpr ModifierKeys(decltype(flags) flags) : flags{ flags } { }
    constexpr ModifierKeys(Flags flags) : flags{ flags } { }

    constexpr bool operator==(const ModifierKeys &other) const = default;
    constexpr bool operator==(Flags other) const { return flags == other; }

    constexpr ModifierKeys withoutFlags(decltype(flags) rawFlagsToClear) const { return ModifierKeys(flags & ~rawFlagsToClear); }
    constexpr ModifierKeys withFlags(decltype(flags) rawFlagsToSet) const { return ModifierKeys(flags | rawFlagsToSet); }

    constexpr bool test(decltype(flags) flagsToTest) const { return (flags & flagsToTest) != 0; }

    constexpr u8 getMouseButtonsDownCount() const
    {
      u8 num = 0;

      if (flags & leftButtonModifier)     ++num;
      if (flags & rightButtonModifier)    ++num;
      if (flags & middleButtonModifier)   ++num;

      return num;
    }

    constexpr ModifierKeys &operator^=(decltype(flags) value) { flags ^= value; return *this; }
    constexpr ModifierKeys &operator&=(decltype(flags) value) { flags &= value; return *this; }
    constexpr ModifierKeys &operator|=(decltype(flags) value) { flags |= value; return *this; }

    constexpr operator decltype(flags)() { return flags; }

    // last known state, won't query OS
    //static ModifierKeys getCurrentModifiers();
    // queries for mouse buttons at this point in time
    //static ModifierKeys getCurrentModifiersRealtime();
  };

  enum class MouseCursorTypes
  {
    Normal,                 // Default pointing arrow
    Caret,                  // Caret (I-Beam) for text entry
    Crosshair,              // Cross-hair
    PointingHand,           // Hand with a pointing finger
    Forbidden,              // Operation not allowed
    LeftRightResize,        // Left/right arrow for horizontal resize
    UpDownResize,           // Up/down arrow for vertical resize
    UpLeftDownRightResize,  // Diagonal arrow for down/right resize
    UpRightDownLeftResize,  // Diagonal arrow for down/left resize
    AllScroll,              // Omnidirectional "arrow" for scrolling
  };

  class Component;

  struct MouseEvent
  {
    i32 x;
    i32 y;
    Point<i32> mouseDownPosition;
    Component *eventComponent;
    Component *originalComponent;
    ModifierKeys mods;

    // The amount that the wheel has been moved in the X axis.
    // If isReversed == true/false, then a negative deltaX means that the wheel has been
    // pushed physically to the left/right.
    float wheelDeltaX = 0.0f;

    // The amount that the wheel has been moved in the Y axis.
    // If isReversed == true/false, then a negative deltaY means that the wheel has been
    // pushed physically upwards/downwards.
    float wheelDeltaY = 0.0f;

    // Indicates whether the user has reversed the direction of the wheel.
    // See deltaX and deltaY for an explanation of the effects of this value.
    bool wheelIsReversed = false;

    // If true, then the wheel has continuous, un-stepped motion.
    bool wheelIsSmooth = false;

    // If true, then this event is part of the inertial momentum phase that follows
    // the wheel being released.
    bool wheelIsInertial = false;

    u8 numberOfClicks = 0;

    MouseEvent getEventRelativeTo(Component *otherComponent) const;
    Point<int> getOffsetFromDragStart() const 
    { return { x - mouseDownPosition.x, y - mouseDownPosition.y }; }
  };

  struct KeyPress
  {
    // a code that represents the key - this value must be
    // one of special constants listed in this class, or an
    // 8 - bit character code such as a letter(case is ignored),
    // digit or a simple key like "," or ".". Note that this
    // isn't the same as the textCharacter parameter, so for example
    // a keyCode of 'a' and a shift - key modifier should have a
    // textCharacter value of 'A'.
    i32 keyCode = 0;

    // the modifiers to associate with the keystroke
    ModifierKeys mods;

    // the character that would be printed if someone typed
    // this keypress into a text editor. This value may be
    // null if the keypress is a non - printing character
    u32 textCharacter = 0;

    // Returns true if this keypress is for the given keycode without any modifiers.
    constexpr bool operator==(i32 code) const
    { return keyCode == code && !mods.test(ModifierKeys::allKeyboardModifiers); }

    //==============================================================================
    /** Returns true if this is a valid KeyPress.

        A null keypress can be created by the default constructor, in case it's
        needed.
    */
    constexpr bool isValid() const { return keyCode != 0; }

    //==============================================================================
    /** Checks whether the user is currently holding down the keys that make up this
        KeyPress.

        Note that this will return false if any extra modifier keys are
        down - e.g. if the keypress is CTRL+X and the user is actually holding CTRL+ALT+x
        then it will be false.
    */
    bool isCurrentlyDown() const;

    /** Checks whether a particular key is held down, irrespective of modifiers.

        The values for key codes can either be one of the special constants defined in
        this class, or an 8-bit character code.
    */
    static bool isKeyCurrentlyDown(int keyCode);
  };

  struct Animator
  {
    enum EventType { Hover, Click };

    void tick(bool isHovered, bool isClicked, float hoverIncrement, float clickIncrement)
    {
      if (isHovered)
        hoverValue = utils::min(1.0f, hoverValue + hoverIncrement);
      else
        hoverValue = utils::max(0.0f, hoverValue - hoverIncrement);

      if (isClicked)
        clickValue = utils::min(1.0f, clickValue + clickIncrement);
      else
        clickValue = utils::max(0.0f, clickValue - clickIncrement);
    }
    float getValue(EventType type, float min = 0.0f, float max = 1.0f) const
    {
      float value;
      switch (type)
      {
      case Hover:
        value = hoverValue;
        break;
      case Click:
        value = clickValue;
        break;
      default:
        value = 1.0f;
        break;
      }

      value *= max - min;
      return value + min;
    }

    float hoverValue = 0.0f;
    float clickValue = 0.0f;
  };

  struct MonitorInfo
  {
    void *nativeHandle;
    Rectangle<i32> totalArea;
    Rectangle<i32> workArea;
    float dpiScale;
    bool isPrimary;
  };

  MonitorInfo getCurrentMonitorInfo(void *nativeHandle);
}
