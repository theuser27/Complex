
// Created: 2025-04-06 10:56:01

#include "gui_utils.hpp"

#include "Framework/memory.hpp"
#include "BaseComponent.hpp"

namespace Interface
{
  MouseEvent MouseEvent::getEventRelativeTo(Component *otherComponent) const noexcept
  {
    MouseEvent ret = *this;
    ret.eventComponent = otherComponent;
    auto newPosition = otherComponent->getRelativePoint(eventComponent, Point{ x, y });
    ret.x = newPosition.x;
    ret.y = newPosition.y;
    ret.mouseDownPosition = otherComponent->getRelativePoint(eventComponent, mouseDownPosition);

    return ret;
  }

  Colour Colour::overlaidWith(Colour foregroundColour) const noexcept
  {
    auto destAlpha = a;

    if (destAlpha <= 0)
      return foregroundColour;

    int invA = 0xff - (int)foregroundColour.a;
    int resA = 0xff - (((0xff - destAlpha) * invA) >> 8);

    if (resA <= 0)
      return *this;

    int da = (invA * destAlpha) / resA;

    return Colour
    {
      (u8)(foregroundColour.r + ((((int)r - foregroundColour.r) * da) >> 8)),
      (u8)(foregroundColour.g + ((((int)g - foregroundColour.g) * da) >> 8)),
      (u8)(foregroundColour.b + ((((int)b - foregroundColour.b) * da) >> 8)),
      (u8)resA
    };
  }

  static strict_inline Colour
  premultiply(Colour c)
  {
    if (c.a < 0xff)
    {
      if (c.a == 0)
      {
        c.b = 0;
        c.g = 0;
        c.r = 0;
      }
      else
      {
        c.b = (u8)((c.b * c.a + 0x7f) >> 8);
        c.g = (u8)((c.g * c.a + 0x7f) >> 8);
        c.r = (u8)((c.r * c.a + 0x7f) >> 8);
      }
    }

    return c;
  }

  static strict_inline Colour
  unpremultiply(Colour c)
  {
    if (c.a < 0xff)
    {
      if (c.a == 0)
      {
        c.b = 0;
        c.g = 0;
        c.r = 0;
      }
      else
      {
        c.b = (u8)utils::min((u32)0xffu, (c.b * 0xffu) / c.a);
        c.g = (u8)utils::min((u32)0xffu, (c.g * 0xffu) / c.a);
        c.r = (u8)utils::min((u32)0xffu, (c.r * 0xffu) / c.a);
      }
    }

    return c;
  }

  static strict_inline Colour 
  tween(Colour one, Colour two, u32 amount) noexcept
  {
    u32 c1 = utils::bit_cast<u32>(one);
    u32 c2 = utils::bit_cast<u32>(two);

    auto dEvenBytes = 0x00ff00ff & c1;
    dEvenBytes += ((((0x00ff00ff & c2) - dEvenBytes) * amount) >> 8);
    dEvenBytes &= 0x00ff00ff;

    auto dOddBytes = 0x00ff00ff & (c1 >> 8);
    dOddBytes += ((((0x00ff00ff & (c2 >> 8)) - dOddBytes) * amount) >> 8);
    dOddBytes &= 0x00ff00ff;

    return utils::bit_cast<Colour>((dOddBytes << 8) | dEvenBytes);
  }

  Colour Colour::interpolatedWith(Colour other, float t) const noexcept
  {
    if (t <= 0.0f)
      return *this;

    if (t >= 1.0f)
      return other;

    auto c1 = premultiply(*this);
    auto c2 = premultiply(other);

    c1 = tween(c1, c2, (u32)::roundf(t * 255.0f));

    return unpremultiply(c1);
  }

  Colour Colour::withBrightness(float newBrightness) const noexcept
  {
    auto hsb = nvgHSB(*this);
    hsb.b = newBrightness;
    hsb = nvgHSB(hsb);

    return { hsb.r, hsb.g, hsb.b, a };
  }

  Colour Colour::withRotatedHue(float amountToRotate) const noexcept
  {
    auto hsb = nvgHSB(*this);
    hsb.r += amountToRotate;
    hsb = nvgHSB(hsb);

    return { hsb.r, hsb.g, hsb.b, a };
  }

  utils::string Colour::toString() const
  {
    char temp[30]{};
    u32 colour = getARGB();
    usize size = (usize)::stbsp_snprintf(temp, COMPLEX_ARRAY_SIZE(temp), "%x", colour);

    return utils::string{ utils::generalAllocator, temp, size };
  }

  Colour Colour::fromString(char *integer)
  {
    long colour = ::strtol(integer, nullptr, 0);
    return Colour{ (u32)colour };
  }
}
