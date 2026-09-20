/*
 * Display.cpp
 *
 * Created: 04/11/2014 09:42:47
 *  Author: David
 */

#include <UI/Display.hpp>
#include "Icons/Icons.hpp"

#undef min
#undef max
#undef array
#undef result
#include <algorithm>

#define DEBUG 0
#include "Debug.hpp"

extern UTFT lcd;

const int maxXerror = 8, maxYerror = 8;		// how close (in pixels) the X and Y coordinates of a touch event need to be to the outline of the button for us to allow it

// Static fields of class DisplayField
LcdFont DisplayField::defaultFont = nullptr;
Colour DisplayField::defaultFcolour = white;
Colour DisplayField::defaultBcolour = black;
Colour DisplayField::defaultButtonBorderColour = black;
Colour DisplayField::defaultGradColour = 0;
Colour DisplayField::defaultPressedBackColour = black;
Colour DisplayField::defaultPressedGradColour = 0;
Palette DisplayField::defaultIconPalette = IconPaletteLight;

DisplayField::DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw)
    : y(py), x(px), width(pw), fcolour(defaultFcolour), bcolour(defaultBcolour),
    changed(true), visible(true), underlined(false), border(false), textRows(1),
    uiPage(UiPage::None), next(nullptr)
{
}

void DisplayField::SetTextRows(const char * _ecv_array null t)
{
	unsigned int rows = 1;
	if (t != nullptr)
	{
		while (*t != 0)
		{
			if (*t == '\n')
			{
				++rows;
			}
			++t;
		}
	}
	textRows = rows;
}

void DisplayField::SetPositionAndWidth(PixelNumber newX, PixelNumber newWidth)
{
	if (x == newX && width == newWidth)
	{
		return;
	}
	x = newX;
	width = newWidth;
	changed = true;
}

void DisplayField::SetPosition(PixelNumber x, PixelNumber y)
{
	if (this->x == x && this->y == y)
	{
		return;
	}
	this->x = x;
	this->y = y;
	changed = true;
}

/*static*/ void DisplayField::SetDefaultColours(Colour pf, Colour pb, Colour pbb, Colour pg, Colour pbp, Colour pgp, Palette pal)
{
	defaultFcolour = pf;
	defaultBcolour = pb;
	defaultButtonBorderColour = pbb;
	defaultGradColour = pg;
	defaultPressedBackColour = pbp;
	defaultPressedGradColour = pgp;
	defaultIconPalette = pal;
}

/*static*/ PixelNumber DisplayField::GetTextWidth(const char* _ecv_array s, PixelNumber maxWidth)
{
	lcd.setFont(DisplayField::defaultFont);
	lcd.setTextPos(0, 9999, maxWidth);
	lcd.printf("%s", s);						// dummy print to get text width
	return lcd.getTextX();
}

/*static*/ PixelNumber DisplayField::GetTextWidth(const char* _ecv_array s, PixelNumber maxWidth, size_t maxChars)
{
	lcd.setTextPos(0, 9999, maxWidth);
	lcd.printf("%.*s", maxChars, s);				// dummy print to get text width
	return lcd.getTextX();
}

void DisplayField::Show(bool v)
{
	if (visible != v)
	{
		visible = changed = v;
	}
}

// Find the best match to a touch event in a list of fields
ButtonPress DisplayField::FindEvent(PixelNumber x, PixelNumber y, DisplayField * null p)
{
	int bestError = maxXerror + maxYerror;
	ButtonPress best;
	while (p != nullptr)
	{
		p->CheckEvent(x, y, bestError, best);
		p = p->next;
	}
	return best;
}

void DisplayField::SetColours(Colour pf, Colour pb)
{
	if (fcolour != pf || bcolour != pb)
	{
		fcolour = pf;
		bcolour = pb;
		changed = true;
	}
}

// ButtonPress class methods
ButtonPress::ButtonPress() : button(nullptr), index(0) { }

ButtonPress::ButtonPress(ButtonBase *b, unsigned int pi) : button(b), index(pi) { }

void ButtonPress::Set(ButtonBase *b, unsigned int pi)
{
	button = b;
	index = pi;
}

void ButtonPress::Clear()
{
	button = nullptr;
	index = 0;
}

event_t ButtonPress::GetEvent() const
{
	return button->GetEvent();
}

int ButtonPress::GetIParam() const
{
	return button->GetIParam(index);
}

const char* _ecv_array ButtonPress::GetSParam() const
{
	return button->GetSParam(index);
}

bool ButtonPress::operator==(const ButtonPress& other) const { return button == other.button && index == other.index; }

// Window class methods
Window::Window(Colour pb)
	: root(nullptr), next(nullptr), backgroundColour(pb)
{
}

// Prepend a field to the linked list of displayed fields
void Window::AddField(DisplayField *d)
{
	d->parent = this;
	d->next = root;
	root = d;
}

bool Window::ObscuredByPopup(const DisplayField *p) const
{
	return next != nullptr
			&& (  (   p->GetMaxY() >= next->Ypos() && p->GetMinY() < next->Ypos() + next->GetHeight()
				   && p->GetMaxX() >= next->Xpos() && p->GetMinX() < next->Xpos() + next->GetWidth()
				  )
				|| next->ObscuredByPopup(p)
			   );
}

bool Window::Visible(const DisplayField *p) const
{
	return p->IsVisible() && !ObscuredByPopup(p);
}

// Get the field that has been touched, or nullptr if we can't find one
ButtonPress Window::FindEvent(PixelNumber x, PixelNumber y)
{
	return (next != nullptr) ? next->FindEvent(x, y)
			: (x < Xpos() || y < Ypos()) ? ButtonPress()
				: DisplayField::FindEvent(x - Xpos(), y - Ypos(), root);
}

// Get the field that has been touched, but search only outside the popup
ButtonPress Window::FindEventOutsidePopup(PixelNumber x, PixelNumber y)
{
	if (next == nullptr) return ButtonPress();

	ButtonPress f = DisplayField::FindEvent(x, y, root);
	return (f.IsValid() && Visible(f.GetButton())) ? f : ButtonPress();
}

void Window::SetPopup(PopupWindow * p, PixelNumber px, PixelNumber py, bool redraw, const PixelNumber displayX, const PixelNumber displayY)
{
	if (px == AutoPlace)
	{
		px = (displayX - p->GetWidth())/2;
	}
	if (py == AutoPlace)
	{
		py = (displayY - p->GetHeight())/2;
	}
	p->SetPos(px, py);
	Window *pw = this;
	while (pw->next != nullptr)
	{
		if (pw->next == p)
		{
			if (redraw)
			{
				p->Refresh(true);
			}
			return;				// popup is already displayed
		}
		pw = pw->next;
	}
	p->next = nullptr;			// ensure no nested popup
	pw->next = p;
	if (redraw)
	{
		p->Refresh(true);
	}
}

void Window::ClearPopup(bool redraw, PopupWindow *whichOne)
{
	if (next != nullptr)
	{
		// Find the penultimate window
		Window *pw = this;
		while (pw->next->next != nullptr)
		{
			pw = pw->next;
		}

		if (whichOne == nullptr || whichOne == pw->next)
		{
			const PixelNumber xmin = pw->next->Xpos(), xmax = xmin + pw->next->GetWidth() - 1, ymin = pw->next->Ypos(), ymax = ymin + pw->next->GetHeight() - 1;
			bool popupWasContained = pw->Contains(xmin, ymin, xmax, ymax);
			if (popupWasContained)
			{
				// Clear the area that was occupied by the last window to the background colour of the penultimate window
				lcd.setColor(pw->backgroundColour);
				lcd.fillRoundRect(xmin, ymin, xmax, ymax);
			}

			// Detach the last window
			pw->next = nullptr;

			if (redraw)
			{
				if (popupWasContained)
				{
					// Re-display the fields of the penultimate window that were obscured
					for (DisplayField * null pp = pw->root; pp != nullptr; pp = pp->next)
					{
						if (pp->IsVisible())
						{
							pp->Refresh(true, pw->Xpos(), pw->Ypos());
						}
					}
				}
				else
				{
					Refresh(true);		// redraw everything
				}
			}
		}
	}
}

bool Window::IsPopupActive(const PopupWindow *popup)
{
	for (PopupWindow *pw = next; pw; pw = pw->next)
	{
		if (pw == popup)
		{
			return true;
		}
	}
	return false;
}


// Redraw the specified field
void Window::Redraw(DisplayField *f)
{
	for (DisplayField * null p = root; p != nullptr; p = p->next)
	{
		if (p == f)
		{
			// The field belongs to this window
			if (!ObscuredByPopup(p))
			{
				if (p->IsVisible())
				{
					p->Refresh(true, Xpos(), Ypos());
				}
				else
				{
					lcd.setColor(backgroundColour);
					lcd.fillRect(p->GetMinX() + Xpos(), p->GetMinY() + Ypos(), p->GetMaxX() + Xpos(), p->GetMaxY() + Ypos());
				}
			}
			return;
		}
	}

	// Else we didn't find the field in our window, so look in nested windows
	if (next != nullptr)
	{
		next->Redraw(f);
	}
}

void Window::Show(DisplayField * null f, bool v)
{
	if (f != nullptr && (f->IsVisible() != v || f->HasChanged()))
	{
		f->Show(v);

		// Check whether the field is currently in the display list, if so then show or hide it
		for (DisplayField *p = root; p != nullptr; p = p->next)
		{
			if (p == f)
			{
				if (ObscuredByPopup(f))
				{
					// nothing to do
				}
				else if (v)
				{
					f->Refresh(true, Xpos(), Ypos());
				}
				else
				{
					lcd.setColor(backgroundColour);
					lcd.fillRect(f->GetMinX(), f->GetMinY(), f->GetMaxX(), f->GetMaxY());
				}
				return;
			}
		}

		// Else we didn't find it, so maybe it is in a popup field
		if (next != nullptr)
		{
			next->Redraw(f);
		}
	}
}

// Show the button as pressed or not
void Window::Press(ButtonPress bp, bool v)
{
	if (bp.IsValid())
	{
		bp.GetButton()->Press(v, bp.GetIndex());
		if (bp.GetButton()->IsVisible())		// need to check this in case we are releasing the button and it has gone invisible since we pressed it
		{
			Redraw(bp.GetButton());
		}
	}
}

MainWindow::MainWindow() : Window(black), staticLeftMargin(0)
{
}

void MainWindow::Init(Colour bc)
{
	backgroundColour = bc;
}

// Refresh all fields. If 'full' is true then we rewrite them all, else we just rewrite those that have changed.
void MainWindow::Refresh(bool full)
{
	if (full)
	{
		lcd.fillScr(backgroundColour, staticLeftMargin);
	}

	for (DisplayField * null pp = root; pp != nullptr; pp = pp->next)
	{
		if (Visible(pp))
		{
			pp->Refresh(full, 0, 0);
		}
	}
	if (next != nullptr)
	{
		next->Refresh(full);
	}
}

bool MainWindow::Contains(PixelNumber xmin, PixelNumber ymin, PixelNumber xmax, PixelNumber ymax) const
{
	UNUSED(xmin); UNUSED(ymin); UNUSED(xmax); UNUSED(ymax);
	return true;
}

void MainWindow::ClearAllPopups()
{
	while (next != nullptr)
	{
		ClearPopup(true);
	}
}

PopupWindow::PopupWindow(PixelNumber ph, PixelNumber pw, Colour pb, Colour pBorder, bool roundCorners)
	: Window(pb), height(ph), width(pw), borderColour(pBorder), roundedCorners(roundCorners)
{
}

void PopupWindow::Refresh(bool full)
{
	if (full)
	{
		// Draw a rectangle inside the border
		lcd.setColor(backgroundColour);
		if (roundedCorners)
		{
			lcd.fillRoundRect(xPos + 1, yPos + 2, xPos + width - 2, yPos + height - 3);
		}
		else
		{
			lcd.fillRect(xPos, yPos, xPos + width, yPos + height);
		}

		// Draw a double border
		lcd.setColor(borderColour);
		if (roundedCorners)
		{
			lcd.drawRoundRect(xPos, yPos, xPos + width - 1, yPos + height - 1);
			lcd.drawRoundRect(xPos + 1, yPos + 1, xPos + width - 2, yPos + height - 2);
		}
		else
		{
			lcd.drawRect(xPos, yPos, xPos + width - 1, yPos + height - 1);
			lcd.drawRect(xPos + 1, yPos + 1, xPos + width - 2, yPos + height - 2);
		}
	}

	for (DisplayField * null p = root; p != nullptr; p = p->next)
	{
		if (p->IsVisible() && (full || !ObscuredByPopup(p)))
		{
			p->Refresh(full, xPos, yPos);
		}
	}

	if (next != nullptr)
	{
		next->Refresh(full);
	}
}

bool PopupWindow::Contains(PixelNumber xmin, PixelNumber ymin, PixelNumber xmax, PixelNumber ymax) const
{
	return xPos + 2 <= xmin && yPos + 2 <= ymin && xPos + width >= xmax + 3 && yPos + height >= ymax + 3;
}

void ColourGradientField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full)
	{
		PixelNumber px = x + xOffset;
		const PixelNumber py = y + yOffset;
		const PixelNumber lineRepeat = width/128;
		for (PixelNumber i = 0; i < 32; ++i)
		{
			lcd.setColor(i << 11);
			for (PixelNumber j = 0; j < lineRepeat; ++j)
			{
				lcd.drawLine(px, py, px, py + height - 1);
				++px;
			}
		}
		for (PixelNumber i = 0; i < 64; ++i)
		{
			lcd.setColor(i << 5);
			for (PixelNumber j = 0; j < lineRepeat; ++j)
			{
				lcd.drawLine(px, py, px, py + height - 1);
				++px;
			}
		}
		for (PixelNumber i = 0; i < 32; ++i)
		{
			lcd.setColor(i);
			for (PixelNumber j = 0; j < lineRepeat; ++j)
			{
				lcd.drawLine(px, py, px, py + height - 1);
				++px;
			}
		}
	}
}

PixelNumber FieldWithText::GetHeight() const
{
	PixelNumber height = UTFT::GetFontHeight(font) * textRows;
	height += (textRows - 1) * 2;		// 2px space between lines
	if (underlined)
	{
		height += 2;					// one space and the underline
	}
	if (border)
	{
		height += 4;					// one space abd border top and bottom
	}
	return height;
}

void FieldWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		xOffset += x;
		yOffset += y;
		PixelNumber textWidth = width;
		if (border)
		{
			if (full)
			{
				lcd.setColor(fcolour);
				lcd.drawRect(xOffset, yOffset, xOffset + width - 1, yOffset + GetHeight() - 1);
			}
			xOffset += 2;
			yOffset += 2;
			textWidth -= 4;
		}

		lcd.setFont(font);
		lcd.setColor(fcolour);
		lcd.setBackColor(bcolour);

		// Do a dummy print to get the text width. Needed for underlining and for centre- or right-aligned text.
		lcd.setTextPos(0, 9999, textWidth);
		PrintText();
		const PixelNumber actualWidth = lcd.getTextX();
		const PixelNumber underlineY = yOffset + UTFT::GetFontHeight(font) + 1;
		if (underlined)
		{
			// Remove previous underlining
			lcd.setColor(bcolour);
			lcd.drawLine(xOffset, underlineY, xOffset + textWidth - 1, underlineY);
			lcd.setColor(fcolour);
		}

		lcd.setTextPos(xOffset, yOffset, xOffset + textWidth);
		if (align == TextAlignment::Left)
		{
			PrintText();
			lcd.clearToMargin();
			if (underlined)
			{
				lcd.drawLine(xOffset, underlineY, xOffset + actualWidth, underlineY);
			}
		}
		else
		{
			lcd.clearToMargin();
			PixelNumber spare = textWidth - actualWidth;
			if (align == TextAlignment::Centre)
			{
				const PixelNumber textX = xOffset + spare/2;
				lcd.setTextPos(textX, yOffset, xOffset + textWidth);
				PrintText();
				if (underlined)
				{
					lcd.drawLine(textX, underlineY, textX + actualWidth - 1, underlineY);
				}
			}
			else
			{
				// Must be right aligned. Try to add a right margin of up to 3 pixels for better appearance.
				if (spare <= 3)
				{
					spare = 0;
				}
				else
				{
					spare -= 3;
				}
				const PixelNumber textX = xOffset + spare;
				lcd.setTextPos(textX, yOffset, xOffset + textWidth);
				PrintText();
				if (underlined)
				{
					lcd.drawLine(textX, underlineY, textX + actualWidth, underlineY);
				}
			}
		}
		changed = false;
	}
}

void TextField::PrintText() const
{
	if (label != nullptr)
	{
		lcd.printf("%s", label);
	}
	if (text != nullptr)
	{
		lcd.printf("%s", text);
	}
}

void FloatField::PrintText() const
{
	if (label != nullptr)
	{
		lcd.printf("%s", label);
	}
	lcd.printf("%.*f", numDecimals, static_cast<double>(val));
	if (units != nullptr)
	{
		lcd.printf("%s", units);
	}
}

void IntegerField::PrintText() const
{
	if (label != nullptr)
	{
		lcd.printf("%s", label);
	}
	lcd.printf("%d", val);
	if (units != nullptr)
	{
		lcd.printf("%s", units);
	}
}

void StaticTextField::PrintText() const
{
	if (text != nullptr)
	{
		lcd.printf("%s", text);
	}
}

ModernTextButton::ModernTextButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		const char * _ecv_array null pt, event_t e, int param, LcdFont pf, bool borderVisible, TextAlignment pa)
	: SingleButton(py, px, pw), text(pt), height(ph), font(pf != nullptr ? pf : DisplayField::defaultFont), drawBorder(borderVisible), alignment(pa)
{
	SetEvent(e, param);
}

void ModernTextButton::SetText(const char * _ecv_array null pt)
{
	if (text != pt)
	{
		text = pt;
		changed = true;
	}
	else
	{
		// The modern UI commonly points at mutable String<> buffers. The pointer may
		// therefore stay the same while the rendered text changes.
		changed = true;
	}
}

void ModernTextButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(pressed ? pressedBackColour : bcolour);
	lcd.fillRoundRect(left, top, right, bottom);
	if (drawBorder)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}

	if (text != nullptr)
	{
		// Match the mock-up: an active/pressed tab fills with the Accent colour
		// and switches to dark text for contrast, instead of keeping the same
		// light text on a red background.
		const Colour activeTextColour = UTFT::fromRGB(18, 22, 28);   // #12161c
		lcd.setTransparentBackground(true);
		lcd.setColor(pressed ? activeTextColour : fcolour);
		lcd.setFont(font);
		lcd.setTextPos(0, 9999, width - 8);
		lcd.printf("%s", text);
		const PixelNumber textWidth = lcd.getTextX();
		const PixelNumber fontHeight = UTFT::GetFontHeight(font);
		PixelNumber tx;
		if (alignment == TextAlignment::Left)
		{
			tx = left + 20;
		}
		else if (alignment == TextAlignment::Right)
		{
			tx = (width > textWidth + 20) ? right - textWidth - 19 : left + 2;
		}
		else
		{
			tx = left + ((width > textWidth) ? (width - textWidth)/2 : 2);
		}
		const PixelNumber ty = top + ((height > fontHeight) ? (height - fontHeight)/2 : 0);
		lcd.setTextPos(tx, ty, right - 3);
		lcd.printf("%s", text);
		lcd.setTransparentBackground(false);
	}
	changed = false;
}

ModernResourceLabel::ModernResourceLabel(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		const char * _ecv_array null pt, LcdFont pf)
	: DisplayField(py, px, pw), text(pt), height(ph), font(pf != nullptr ? pf : DisplayField::defaultFont), icon(ModernResourceIcon::None)
{
}

void ModernResourceLabel::SetText(const char * _ecv_array null pt)
{
	text = pt;
	changed = true;
}

void ModernResourceLabel::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	// Clear only the label band. This also removes a bed glyph when a paged
	// column is reused for a tool on another page.
	lcd.setColor(bcolour);
	lcd.fillRect(left, top, right, bottom);

	lcd.setFont(font);
	lcd.setTransparentBackground(true);
	lcd.setColor(fcolour);
	lcd.setTextPos(0, 9999, width - 4);
	if (text != nullptr)
	{
		lcd.printf("%s", text);
	}
	const PixelNumber textWidth = lcd.getTextX();
	const PixelNumber fontHeight = UTFT::GetFontHeight(font);
	const PixelNumber iconWidth = (icon == ModernResourceIcon::Bed) ? 26
		: (icon == ModernResourceIcon::Chamber) ? 28 : 0;
	const PixelNumber gap = (iconWidth != 0 && textWidth != 0) ? 6 : 0;
	const PixelNumber groupWidth = iconWidth + gap + textWidth;
	const PixelNumber groupLeft = left + ((width > groupWidth) ? (width - groupWidth)/2 : 2);

	if (icon == ModernResourceIcon::Bed)
	{
		const int ix = static_cast<int>(groupLeft);
		const int iy = static_cast<int>(top + ((height > 26) ? (height - 26)/2 : 0));
		// Bed plate. Scaled 30% larger than the original 20px glyph.
		lcd.fillRoundRect(ix, iy + 20, ix + 25, iy + 25);
		// Three compact heat-wave strokes, matching the SVG motif without
		// requiring a new icon asset or palette entry.
		for (int wave = 0; wave < 3; ++wave)
		{
			const int wx = ix + 4 + wave * 9;
			lcd.drawLine(wx, iy + 16, wx + 3, iy + 10);
			lcd.drawLine(wx + 3, iy + 10, wx, iy + 5);
		}
	}
	else if (icon == ModernResourceIcon::Chamber)
	{
		// Chamber motif from the final TOOLS mock-up: a compact square with
		// three right-facing heat/air waves inside. Keep it vector-only so the
		// glyph follows the live foreground/Accent/fault colour.
		const int ix = static_cast<int>(groupLeft);
		const int iy = static_cast<int>(top + ((height > 28) ? (height - 28)/2 : 0));
		lcd.drawRoundRect(ix + 1, iy + 1, ix + 26, iy + 26);
		lcd.drawRoundRect(ix + 2, iy + 2, ix + 25, iy + 25);
		for (int wave = 0; wave < 3; ++wave)
		{
			const int wx = ix + 8 + wave * 5;
			lcd.drawLine(wx, iy + 7, wx + 3, iy + 10);
			lcd.drawLine(wx + 3, iy + 10, wx + 4, iy + 14);
			lcd.drawLine(wx + 4, iy + 14, wx + 3, iy + 18);
			lcd.drawLine(wx + 3, iy + 18, wx, iy + 21);
		}
	}

	if (text != nullptr)
	{
		const PixelNumber tx = groupLeft + iconWidth + gap;
		const PixelNumber ty = top + ((height > fontHeight) ? (height - fontHeight)/2 : 0);
		lcd.setTextPos(tx, ty, right - 2);
		lcd.printf("%s", text);
	}
	lcd.setTransparentBackground(false);
	changed = false;
}

ModernTemperatureButton::ModernTemperatureButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		const char * _ecv_array null pt, ModernTemperatureIcon pi, event_t e, int param, LcdFont pf, bool borderVisible)
	: SingleButton(py, px, pw), text(pt), height(ph), font(pf != nullptr ? pf : DisplayField::defaultFont), icon(pi), drawBorder(borderVisible)
{
	SetEvent(e, param);
}

void ModernTemperatureButton::SetText(const char * _ecv_array null pt)
{
	text = pt;
	changed = true;
}

void ModernTemperatureButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	const Colour background = pressed ? pressedBackColour : bcolour;
	lcd.setColor(background);
	lcd.fillRoundRect(left, top, right, bottom);
	if (drawBorder)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}

	// Small vector status glyph in the same position used by the SVG mock-up.
	const int glyphX = static_cast<int>(left) + 20;
	const int glyphY = static_cast<int>(top) + 18;
	lcd.setColor(fcolour);
	if (icon == ModernTemperatureIcon::Active)
	{
		lcd.drawCircle(glyphX, glyphY, 9);
		lcd.fillCircle(glyphX, glyphY, 3);
	}
	else
	{
		lcd.fillCircle(glyphX, glyphY, 9);
		lcd.setColor(background);
		lcd.fillCircle(glyphX + 5, glyphY - 5, 8);
	}

	if (text != nullptr)
	{
		lcd.setTransparentBackground(true);
		lcd.setColor(fcolour);
		lcd.setFont(font);
		lcd.setTextPos(0, 9999, width - 8);
		lcd.printf("%s", text);
		const PixelNumber textWidth = lcd.getTextX();
		const PixelNumber fontHeight = UTFT::GetFontHeight(font);
		const PixelNumber tx = left + ((width > textWidth) ? (width - textWidth)/2 : 2);
		const PixelNumber ty = top + ((height > fontHeight) ? (height - fontHeight)/2 + 10 : 0);
		lcd.setTextPos(tx, ty, right - 3);
		lcd.printf("%s", text);
		lcd.setTransparentBackground(false);
	}
	changed = false;
}

ModernPowerButton::ModernPowerButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		event_t e, int param, bool borderVisible)
	: SingleButton(py, px, pw), height(ph), drawBorder(borderVisible)
{
	SetEvent(e, param);
}

void ModernPowerButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(pressed ? pressedBackColour : bcolour);
	lcd.fillRoundRect(left, top, right, bottom);
	if (drawBorder)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}

	const int cx = static_cast<int>(left + width/2);
	const int cy = static_cast<int>(top + height/2) + 1;
	lcd.setColor(fcolour);
	// 25% bigger than the original r=10/length=11 glyph, and the ring/line
	// strokes are three passes wide instead of two (50% thicker), with the
	// tile itself left at its original size.
	lcd.drawCircle(cx, cy, 13);
	lcd.drawCircle(cx, cy, 12);
	lcd.drawCircle(cx, cy, 11);
	lcd.drawLine(cx - 1, cy - 16, cx - 1, cy - 2);
	lcd.drawLine(cx,     cy - 16, cx,     cy - 2);
	lcd.drawLine(cx + 1, cy - 16, cx + 1, cy - 2);
	changed = false;
}

ModernStopButton::ModernStopButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		event_t e, LcdFont pf)
	: SingleButton(py, px, pw), height(ph), font(pf != nullptr ? pf : DisplayField::defaultFont)
{
	SetEvent(e, 0);
}

void ModernStopButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const Colour railBg = UTFT::fromRGB(18, 22, 28);       // #12161c
	const Colour stopRed = pressed ? UTFT::fromRGB(160, 39, 35) : UTFT::fromRGB(201, 50, 24); // #C93218 fixed safety red
	const Colour stopText = UTFT::fromRGB(245, 245, 245);
	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;

	// A plain rounded-square tile (same fillRoundRect corner treatment as
	// every other content-area tile), filled with the fixed safety red and
	// labelled STOP, matching the mock-up rather than the earlier octagonal
	// road-sign shape.
	lcd.setColor(railBg);
	lcd.fillRect(left, top, right, bottom);
	lcd.setColor(stopRed);
	lcd.fillRoundRect(left, top, right, bottom);

	lcd.setTransparentBackground(true);
	lcd.setColor(stopText);
	lcd.setFont(font);
	lcd.setTextPos(0, 9999, static_cast<PixelNumber>(width) - 12);
	lcd.printf("STOP");
	const PixelNumber textWidth = lcd.getTextX();
	const PixelNumber fontHeight = UTFT::GetFontHeight(font);
	const PixelNumber tx = left + ((width > textWidth) ? (width - textWidth) / 2 : 2);
	const PixelNumber ty = top + ((height > fontHeight) ? (height - fontHeight) / 2 : 0);
	lcd.setTextPos(tx, ty, right - 3);
	lcd.printf("STOP");
	lcd.setTransparentBackground(false);
	changed = false;
}


ModernAlertNavButton::ModernAlertNavButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		event_t e)
	: SingleButton(py, px, pw), height(ph)
{
	SetEvent(e, 0);
}

void ModernAlertNavButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const Colour railBg = UTFT::fromRGB(18, 22, 28);       // #12161c
	const Colour pressedBg = UTFT::fromRGB(28, 34, 43);    // #1c222b
	const Colour alertRed = UTFT::fromRGB(201, 50, 24);    // #C93218 semantic alert red
	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;

	lcd.setColor(pressed ? pressedBg : railBg);
	lcd.fillRoundRect(left, top, right, bottom);

	const int cx = static_cast<int>(left + width/2);
	const int triangleTop = static_cast<int>(top) + 7;
	const int triangleBottom = static_cast<int>(bottom) - 7;
	const int halfWidth = 30;
	lcd.setColor(alertRed);
	lcd.drawLine(cx, triangleTop, cx - halfWidth, triangleBottom);
	lcd.drawLine(cx - halfWidth, triangleBottom, cx + halfWidth, triangleBottom);
	lcd.drawLine(cx + halfWidth, triangleBottom, cx, triangleTop);
	// Double the outline so it reads clearly on the 7-inch panel.
	lcd.drawLine(cx, triangleTop + 2, cx - halfWidth + 3, triangleBottom - 2);
	lcd.drawLine(cx - halfWidth + 3, triangleBottom - 2, cx + halfWidth - 3, triangleBottom - 2);
	lcd.drawLine(cx + halfWidth - 3, triangleBottom - 2, cx, triangleTop + 2);
	// Soften the three sharp vertices with a small filled dot at each corner,
	// approximating the slightly rounded triangle edges from the mock-up
	// without needing a dedicated rounded-polygon primitive.
	lcd.fillCircle(cx, triangleTop, 2);
	lcd.fillCircle(cx - halfWidth, triangleBottom, 2);
	lcd.fillCircle(cx + halfWidth, triangleBottom, 2);

	// Vector exclamation mark.
	lcd.fillRoundRect(cx - 2, triangleTop + 13, cx + 2, triangleBottom - 13);
	lcd.fillCircle(cx, triangleBottom - 7, 2);
	changed = false;
}

ModernMasterNavButton::ModernMasterNavButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		MasterNavIcon pi, event_t e)
	: SingleButton(py, px, pw), icon(pi), height(ph)
{
	SetEvent(e, 0);
}

void ModernMasterNavButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const Colour tileBg = UTFT::fromRGB(28, 34, 43);          // #1c222b, inactive tile
	const Colour glyphLight = UTFT::fromRGB(154, 164, 178);   // inactive glyph
	const Colour glyphDark = UTFT::fromRGB(18, 22, 28);       // #12161c, glyph on active fill
	const bool active = pressed;
	const Colour fill = active ? bcolour : tileBg;            // bcolour already carries the current Accent colour
	const Colour glyph = active ? glyphDark : glyphLight;

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	const int cx = static_cast<int>(left + width / 2);
	const int cy = static_cast<int>(top + height / 2);

	lcd.setColor(fill);
	lcd.fillRoundRect(left, top, right, bottom);
	lcd.setColor(glyph);

	switch (icon)
	{
	case MasterNavIcon::Joystick:
		// Base, stick, and ball - matches the SVG joystick glyph.
		lcd.fillRoundRect(cx - 13, cy + 9, cx + 13, cy + 19);
		lcd.fillRect(cx - 2, cy - 9, cx + 2, cy + 10);
		lcd.fillCircle(cx, cy - 13, 8);
		break;

	case MasterNavIcon::List:
		// Three bullet-and-line rows.
		for (int row = -1; row <= 1; ++row)
		{
			const int ry = cy + row * 9;
			lcd.fillCircle(cx - 11, ry, 3);
			lcd.fillRect(cx - 3, ry - 2, cx + 13, ry + 2);
		}
		break;

	case MasterNavIcon::Gear:
	default:
		// Body, eight radial teeth, and a hole punched back to the fill
		// colour. Scaled up slightly for the larger 76px rail tile, with
		// rounded teeth for a less blocky silhouette than plain squares.
		lcd.fillCircle(cx, cy, 14);
		{
			static const int dx[8] = { 0, 13, 18, 13, 0, -13, -18, -13 };
			static const int dy[8] = { -18, -13, 0, 13, 18, 13, 0, -13 };
			for (unsigned int i = 0; i < 8; ++i)
			{
				lcd.fillRoundRect(cx + dx[i] - 4, cy + dy[i] - 4, cx + dx[i] + 4, cy + dy[i] + 4);
			}
		}
		lcd.setColor(fill);
		lcd.fillCircle(cx, cy, 6);
		break;
	}

	changed = false;
}

ModernIconButton::ModernIconButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		Icon pi, event_t e, int param, bool borderVisible)
	: SingleButton(py, px, pw), icon(pi), height(ph), drawBorder(borderVisible)
{
	SetEvent(e, param);
}

void ModernIconButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}
	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(pressed ? pressedBackColour : bcolour);
	lcd.fillRoundRect(left, top, right, bottom);
	if (drawBorder)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}
	// Cancel/OK are drawn as vector glyphs rather than the legacy fixed-size
	// bitmap icons, so the X and checkmark scale and stay centred cleanly at
	// any button size (95x58, 110x78, ...) instead of looking small/off in a
	// bigger tile. This keeps the look identical across every popup that
	// uses IconCancel/IconOk.
	if (icon == IconCancel || icon == IconOk)
	{
		const int cx = static_cast<int>(left + width / 2);
		const int cy = static_cast<int>(top + height / 2);
		const int span = static_cast<int>((width < height) ? width : height);
		const int r = span / 2 - 12;
		lcd.setColor(fcolour);
		if (icon == IconCancel)
		{
			for (int o = -1; o <= 1; ++o)
			{
				lcd.drawLine(cx - r + o, cy - r, cx + r + o, cy + r);
				lcd.drawLine(cx - r, cy - r + o, cx + r, cy + r + o);
				lcd.drawLine(cx + r + o, cy - r, cx - r + o, cy + r);
				lcd.drawLine(cx + r, cy - r + o, cx - r, cy + r + o);
			}
		}
		else
		{
			const int shortArm = static_cast<int>(r * 0.7);
			for (int o = -1; o <= 1; ++o)
			{
				lcd.drawLine(cx - shortArm, cy + o, cx - shortArm / 3, cy + r + o);
				lcd.drawLine(cx - shortArm / 3, cy + r + o, cx + r, cy - r + o);
			}
		}
		changed = false;
		return;
	}
	// Paging arrows are vector-drawn so they scale cleanly with the
	// modern paging tiles and require no bitmap storage.
	if (icon == IconUp || icon == IconDown)
	{
		const int cx = static_cast<int>(left + width / 2);
		const int cy = static_cast<int>(top + height / 2);
		const int r = 12;

		lcd.setColor(fcolour);
		for (int o = -1; o <= 1; ++o)
		{
			if (icon == IconUp)
			{
				lcd.drawLine(cx - r, cy + 6 + o, cx, cy - 6 + o);
				lcd.drawLine(cx, cy - 6 + o, cx + r, cy + 6 + o);
			}
			else
			{
				lcd.drawLine(cx - r, cy - 6 + o, cx, cy + 6 + o);
				lcd.drawLine(cx, cy + 6 + o, cx + r, cy - 6 + o);
			}
		}

		changed = false;
		return;
	}

	const PixelNumber iw = GetIconWidth(icon);
	const PixelNumber ih = GetIconHeight(icon);
	lcd.setTransparentBackground(true);
	lcd.drawBitmap4(left + (width - iw)/2, top + (height - ih)/2, iw, ih, GetIconData(icon), defaultIconPalette);
	lcd.setTransparentBackground(false);
	changed = false;
}

ModernHomeButton::ModernHomeButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		const char * _ecv_array null plabel, event_t e, int param, LcdFont pf, bool borderVisible)
	: SingleButton(py, px, pw), label(plabel), height(ph), drawBorder(borderVisible),
	  font(pf != nullptr ? pf : DisplayField::defaultFont)
{
	SetEvent(e, param);
}

void ModernHomeButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(pressed ? pressedBackColour : bcolour);
	lcd.fillRoundRect(left, top, right, bottom);
	if (drawBorder)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}

	// House outline.  It is drawn from primitives rather than the legacy
	// palette bitmap so the whole glyph can follow the live Accent colour.
	const int cx = static_cast<int>(left + width/2);
	const int peakY = static_cast<int>(top) + 14;
	const int roofY = static_cast<int>(top) + 34;
	const int baseY = static_cast<int>(top) + height - 14;
	const int roofHalf = 29;	// 24 * 1.2, per paneldue_home_icon_wider.svg
	const int wallHalf = 19;	// 16 * 1.2, per paneldue_home_icon_wider.svg
	lcd.setColor(fcolour);
	for (int t = 0; t < 2; ++t)
	{
		lcd.drawLine(cx, peakY + t, cx - roofHalf, roofY + t);
		lcd.drawLine(cx, peakY + t, cx + roofHalf, roofY + t);
		lcd.drawLine(cx - wallHalf - t, roofY - 1, cx - wallHalf - t, baseY);
		lcd.drawLine(cx + wallHalf + t, roofY - 1, cx + wallHalf + t, baseY);
		lcd.drawLine(cx - wallHalf, baseY - t, cx + wallHalf, baseY - t);
	}

	if (label != nullptr)
	{
		lcd.setTransparentBackground(true);
		lcd.setFont(font);
		lcd.setTextPos(0, 9999, static_cast<PixelNumber>(2 * wallHalf));
		lcd.printf("%s", label);
		const PixelNumber textWidth = lcd.getTextX();
		const PixelNumber fontHeight = UTFT::GetFontHeight(font);
		const PixelNumber tx = static_cast<PixelNumber>(cx - static_cast<int>(textWidth)/2);
		const int fontHeightInt = static_cast<int>(fontHeight);
		const PixelNumber ty = static_cast<PixelNumber>(roofY + ((baseY - roofY > fontHeightInt) ? (baseY - roofY - fontHeightInt)/2 : 0));
		lcd.setTextPos(tx, ty, right - 2);
		lcd.printf("%s", label);
		lcd.setTransparentBackground(false);
	}
	changed = false;
}

ModernBedCompButton::ModernBedCompButton(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		event_t e, int param)
	: SingleButton(py, px, pw), height(ph)
{
	SetEvent(e, param);
}

void ModernBedCompButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}

	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(pressed ? pressedBackColour : bcolour);
	lcd.fillRoundRect(left, top, right, bottom);

	// Approx. 54x38 glyph: about 30% larger than the 41x30 large legacy
	// IconBedComp while retaining the same up/down bed-level visual language.
	const int cx = static_cast<int>(left + width/2);
	const int cy = static_cast<int>(top + height/2);
	lcd.setColor(fcolour);
	lcd.drawLine(cx - 27, cy - 10, cx + 27, cy - 10);
	lcd.drawLine(cx - 27, cy - 9, cx + 27, cy - 9);
	// Up marker, left side.
	lcd.drawLine(cx - 20, cy + 8, cx - 13, cy - 2);
	lcd.drawLine(cx - 13, cy - 2, cx - 6, cy + 8);
	lcd.drawLine(cx - 20, cy + 8, cx - 6, cy + 8);
	// Down marker, right side, mirrored above the line (apex points down,
	// toward the line, matching the left marker's up-and-toward-the-line
	// language instead of sitting in the same half as the left marker).
	lcd.drawLine(cx + 6, cy - 27, cx + 20, cy - 27);
	lcd.drawLine(cx + 6, cy - 27, cx + 13, cy - 17);
	lcd.drawLine(cx + 20, cy - 27, cx + 13, cy - 17);
	changed = false;
}

ModernCard::ModernCard(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
	Colour fillColour, Colour pBorderColour, bool showBorder)
	: DisplayField(py, px, pw), height(ph), borderColour(pBorderColour), borderVisible(showBorder)
{
	bcolour = fillColour;
}

void ModernCard::SetBorderVisible(bool visible)
{
	if (borderVisible != visible)
	{
		borderVisible = visible;
		changed = true;
	}
}

void ModernCard::SetBorderColour(Colour colour)
{
	if (borderColour != colour)
	{
		borderColour = colour;
		changed = true;
	}
}

void ModernCard::SetFillColour(Colour colour)
{
	if (bcolour != colour)
	{
		bcolour = colour;
		changed = true;
	}
}

void ModernCard::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (!full && !changed)
	{
		return;
	}
	const PixelNumber left = x + xOffset;
	const PixelNumber top = y + yOffset;
	const PixelNumber right = left + width - 1;
	const PixelNumber bottom = top + height - 1;
	lcd.setColor(bcolour);
	lcd.fillRoundRect(left, top, right, bottom);
	if (borderVisible)
	{
		lcd.setColor(borderColour);
		lcd.drawRoundRect(left, top, right, bottom);
		if (width > 2 && height > 2)
		{
			lcd.drawRoundRect(left + 1, top + 1, right - 1, bottom - 1);
		}
	}
	changed = false;
}

ButtonBase::ButtonBase(PixelNumber py, PixelNumber px, PixelNumber pw)
	: DisplayField(py, px, pw),
	  borderColour(defaultButtonBorderColour), gradColour(defaultGradColour),
	  pressedBackColour(defaultPressedBackColour), pressedGradColour(defaultPressedGradColour), evt(nullEvent), pressed(false)
{
}

PixelNumber ButtonBase::textMargin = 1;
PixelNumber ButtonBase::iconMargin = 1;

void ButtonBase::DrawOutline(PixelNumber xOffset, PixelNumber yOffset, bool isPressed) const
{
	lcd.setColor((isPressed) ? pressedBackColour : bcolour);
	// Note that we draw the filled rounded rectangle with the full width but 2 pixels less height than the border.
	// This means that we start with the requested colour inside the border.
	lcd.fillRoundRect(x + xOffset, y + yOffset + 1, x + xOffset + width - 1, y + yOffset + GetHeight() - 2, (isPressed) ? pressedGradColour : gradColour, buttonGradStep);
	lcd.setColor(borderColour);
	lcd.drawRoundRect(x + xOffset, y + yOffset, x + xOffset + width - 1, y + yOffset + GetHeight() - 1);
}

void ButtonBase::CheckEvent(PixelNumber x, PixelNumber y, int& bestError, ButtonPress& best) /*override*/
{
	if (IsVisible() && GetEvent() != nullEvent)
	{
		const int xError = (x < GetMinX()) ? GetMinX() - x
								: (x > GetMaxX()) ? x - GetMaxX()
									: 0;
		if (xError < maxXerror)
		{
			const int yError = (y < GetMinY()) ? GetMinY() - y
									: (y > GetMaxY()) ? y - GetMaxY()
										: 0;
			if (yError < maxYerror && xError + yError < bestError)
			{
				bestError = xError + yError;
				best.Set(this, 0);
			}
		}
	}
}

SingleButton::SingleButton(PixelNumber py, PixelNumber px, PixelNumber pw)
	: ButtonBase(py, px, pw)
{
	param.sParam = nullptr;
}

void SingleButton::DrawOutline(PixelNumber xOffset, PixelNumber yOffset) const
{
	ButtonBase::DrawOutline(xOffset, yOffset, pressed);
}

void SingleButton::Press(bool p, int index) /*override*/
{
	UNUSED(index);
	if (p != pressed)
	{
		pressed = p;
		changed = true;
	}
}

/*static*/ LcdFont ButtonWithText::font;

PixelNumber ButtonWithText::GetHeight() const
{
	PixelNumber ret = (UTFT::GetFontHeight(font) + 2) * textRows - 2;	// height of the text
	ret += 2 * textMargin + 2;											// add the border height
	return ret;
}

void ButtonWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DrawOutline(xOffset, yOffset);
		lcd.setTransparentBackground(true);
		lcd.setColor(fcolour);
		lcd.setFont(font);
		unsigned int rowsLeft = textRows;
		size_t offset = 0;
		PixelNumber rowY = y + yOffset + textMargin + 1;
		do
		{
			lcd.setTextPos(0, 9999, width - 6);
			PrintText(offset);							// dummy print to get text width
			PixelNumber spare = width - 6 - lcd.getTextX();
			lcd.setTextPos(x + xOffset + 3 + spare/2, rowY, x + xOffset + width - 3);	// text is always centre-aligned
			offset += PrintText(offset) + 1;
			rowY += UTFT::GetFontHeight(font) + 2;
		} while (--rowsLeft != 0);
		lcd.setTransparentBackground(false);
		changed = false;
	}
}

CharButton::CharButton(PixelNumber py, PixelNumber px, PixelNumber pw, char pc, event_t e)
	: ButtonWithText(py, px, pw)
{
	SetEvent(e, (int)pc);
}

size_t CharButton::PrintText(size_t offset) const
{
	UNUSED(offset);
	return lcd.write((char)GetIParam(0));
}

TextButton::TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * _ecv_array null pt, event_t e, int param)
	: ButtonWithText(py, px, pw), text(pt)
{
	SetTextRows(pt);
	SetEvent(e, param);
}

TextButton::TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * _ecv_array null pt, event_t e, const char * _ecv_array param)
	: ButtonWithText(py, px, pw), text(pt)
{
	SetEvent(e, param);
}

size_t TextButton::PrintText(size_t offset) const
{
	if (text != nullptr)
	{
		return lcd.printf("%s", text + offset);
	}
	return 0;
}

TextButtonWithLabel::TextButtonWithLabel(PixelNumber py, PixelNumber px, PixelNumber pw, const char * _ecv_array null pt, event_t e, int param, const char* _ecv_array null label)
	: TextButton(py - 2, px, pw, pt, e, param), label(label)
{
}

TextButtonWithLabel::TextButtonWithLabel(PixelNumber py, PixelNumber px, PixelNumber pw, const char * _ecv_array null pt, event_t e, const char * _ecv_array param, const char* _ecv_array null label)
	: TextButton(py - 2, px, pw, pt, e, param), label(label)
{
}

size_t TextButtonWithLabel::PrintText(size_t offset) const
{
	size_t w = 0;
	if (label != nullptr)
	{
		w += lcd.printf("%s", label);
	}
	w += TextButton::PrintText(offset);
	return w;
}

IconButton::IconButton(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, int param)
	: SingleButton(py, px, pw), icon(ic)
{
	SetEvent(e, param);
}

IconButton::IconButton(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, const char * _ecv_array param)
: SingleButton(py, px, pw), icon(ic)
{
	SetEvent(e, param);
}

void IconButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DrawOutline(xOffset, yOffset);
		const uint16_t sx = GetIconWidth(icon), sy = GetIconHeight(icon);
		lcd.setTransparentBackground(true);
		lcd.drawBitmap4(xOffset + x + (width - sx)/2, yOffset + y + iconMargin + 1, sx, sy, GetIconData(icon), defaultIconPalette);
		lcd.setTransparentBackground(false);
		changed = false;
	}
}

IconButtonWithText::IconButtonWithText(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, const char * text, int param)
	: IconButton(py, px, pw, ic, e, param), font(DisplayField::defaultFont), text(text), val(0), printText(true), drawIcon(true)
{
}

IconButtonWithText::IconButtonWithText(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, const char * text, const char * _ecv_array param)
	: IconButton(py, px, pw, ic, e, param), font(DisplayField::defaultFont), text(text), val(0), printText(true), drawIcon(true)
{
}

IconButtonWithText::IconButtonWithText(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, int textVal, int param)
	: IconButton(py, px, pw, ic, e, param), font(DisplayField::defaultFont), text(nullptr), val(textVal), printText(true), drawIcon(true)
{
}

IconButtonWithText::IconButtonWithText(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic, event_t e, int textVal, const char * _ecv_array param)
	: IconButton(py, px, pw, ic, e, param), font(DisplayField::defaultFont), text(nullptr), val(textVal), printText(true), drawIcon(true)
{
}

size_t IconButtonWithText::PrintText() const
{
	size_t ret = 0;
	if (!printText)
	{
		return ret;
	}
	if (text != nullptr)
	{
		ret += lcd.printf("%s", text);
	}
	else {
		ret += lcd.printf("%d", val);
	}
	return ret;
}

void IconButtonWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DrawOutline(xOffset, yOffset);
		const uint16_t	sx = GetIconWidth(icon),
						sy = drawIcon ? GetIconHeight(icon) : 0;

		lcd.setFont(font);
		lcd.setTextPos(0, 9999, width - 6);
		PrintText();							// dummy print to get text width
		const PixelNumber textWidth = lcd.getTextX() + 6;	// add three pixels on each side

		// Print the icon
		lcd.setTransparentBackground(true);
		const PixelNumber iconXOffset = xOffset + x + (width - (sx+textWidth))/2;
		if (drawIcon)
		{
			lcd.drawBitmap4(iconXOffset, yOffset + y + iconMargin + 1, sx, sy, GetIconData(icon), defaultIconPalette);
		}

		// Print the text
		const PixelNumber textX = iconXOffset + sx + 3;
		const PixelNumber rowY = y + yOffset + textMargin + 1;
		lcd.setTextPos(textX, rowY, textX + textWidth);
		lcd.setColor(fcolour);
		PrintText();
		lcd.setTransparentBackground(false);

		changed = false;
	}
}

size_t IntegerButton::PrintText(size_t offset) const
{
	UNUSED(offset);
	size_t ret = 0;
	if (label != nullptr)
	{
		ret += lcd.printf("%s", label);
	}
	ret += lcd.printf("%d", val);
	if (units != nullptr)
	{
		ret += lcd.printf("%s", units);
	}
	return ret;
}

size_t FloatButton::PrintText(size_t offset) const
{
	UNUSED(offset);
	size_t ret = lcd.printf("%.*f", numDecimals, static_cast<double>(val));
	if (units != nullptr)
	{
		ret += lcd.printf("%s", units);
	}
	return ret;
}

ButtonRow::ButtonRow(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ps, unsigned int nb, event_t e)
	: ButtonBase(py, px, pw), numButtons(nb), whichPressed(-1), step(ps)
{
	evt = e;
}

/*static*/ LcdFont ButtonRowWithText::font;

ButtonRowWithText::ButtonRowWithText(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ps, unsigned int nb, event_t e)
	: ButtonRow(py, px, pw, ps, nb, e)
{
}

PixelNumber ButtonRowWithText::GetHeight() const
{
	PixelNumber ret = (UTFT::GetFontHeight(font) + 2) * textRows - 2;	// height of the text
	ret += 2 * textMargin + 2;											// add the border height
	return ret;
}

void ButtonRowWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		for (unsigned int i = 0; i < numButtons; ++i)
		{
			const PixelNumber buttonXoffset = xOffset + i * step;
			DrawOutline(buttonXoffset, yOffset, (int)i == whichPressed);
			lcd.setTransparentBackground(true);
			lcd.setColor(fcolour);
			lcd.setFont(font);
			lcd.setTextPos(0, 9999, width - 6);
			PrintText(i);							// dummy print to get text width
			PixelNumber spare = width - 6 - lcd.getTextX();
			lcd.setTextPos(x + buttonXoffset + 3 + spare/2, y + yOffset + textMargin + 1, x + buttonXoffset + width - 3);	// text is always centre-aligned
			PrintText(i);
			lcd.setTransparentBackground(false);
		}
		changed = false;
	}
}

void CharButtonRow::PrintText(unsigned int n) const
{
	lcd.write(text[n]);
}

CharButtonRow::CharButtonRow(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ps, const char * _ecv_array s, event_t e)
	: ButtonRowWithText(py, px, pw, ps, strlen(s), e), text(s)
{
}

void CharButtonRow::CheckEvent(PixelNumber x, PixelNumber y, int& bestError, ButtonPress& best) /*override*/
{
	if (visible && GetEvent() != nullEvent)
	{
		const int yError = (y < GetMinY()) ? GetMinY() - y
								: (y > GetMaxY()) ? y - GetMaxY()
									: 0;
		if (yError < maxYerror && yError < bestError)
		{
			PixelNumber minX = GetMinX();
			PixelNumber maxX = GetMaxX();
			for (size_t i = 0; i < numButtons; ++i)
			{
				const int xError = (x < minX) ? minX - x
										: (x > maxX) ? x - maxX
											: 0;
				if (xError < maxXerror && xError + yError < bestError)
				{
					bestError = xError + yError;
					best.Set(this, i);
				}
				minX += step;
				maxX += step;
			}
		}
	}
}

void CharButtonRow::Press(bool p, int index) /*override*/
{
	whichPressed = (p) ? index : -1;
}

void CharButtonRow::ChangeText(const char* _ecv_array s)
{
	if (strcmp(text, s) == 0)
	{
		return;
	}
	text = s;
	changed = true;
}

void ProgressBar::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		PixelNumber pixelsSet = ((width - 2) * percent)/100;
		if (full)
		{
			lcd.setColor(fcolour);
			lcd.drawLine(x + xOffset, y, x + xOffset + width - 1, y + yOffset);
			lcd.drawLine(x + xOffset, y + yOffset + height - 1, x + xOffset + width - 1, y + yOffset + height - 1);
			lcd.drawLine(x + xOffset + width - 1, y + yOffset + 1, x + xOffset + width - 1, y + yOffset + height - 2);

			lcd.fillRect(x + xOffset, y + yOffset + 1, x + xOffset + pixelsSet, y + yOffset + height - 2);
			if (pixelsSet < width - 2)
			{
				lcd.setColor(bcolour);
				lcd.fillRect(x + xOffset + pixelsSet + 1, y + yOffset + 1, x + xOffset + width - 2, y + yOffset + height - 2);
			}
		}
		else if (pixelsSet > lastNumPixelsSet)
		{
			lcd.setColor(fcolour);
			lcd.fillRect(x + xOffset + lastNumPixelsSet, y + yOffset + 1, x + xOffset + pixelsSet, y + yOffset + height - 2);
		}
		else if (pixelsSet < lastNumPixelsSet)
		{
			lcd.setColor(bcolour);
			lcd.fillRect(x + xOffset + pixelsSet + 1, y + yOffset + 1, x + xOffset + lastNumPixelsSet, y + yOffset + height - 2);
		}
		changed = false;
		lastNumPixelsSet = pixelsSet;
	}
}

void StaticImageField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		lcd.drawCompressedBitmap(x + xOffset, y + yOffset, width, height, data);
		changed = false;
	}
}

void DrawDirect::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	// nothing todo
	UNUSED(full); UNUSED(xOffset); UNUSED(yOffset);

	if (refreshNotify)
		refreshNotify(full, changed);

	changed = false;
}

void DrawDirect::DrawRect(PixelNumber widthRect, PixelNumber heightRect, unsigned int pixels_offset, const qoi_rgba_t *pixels, size_t pixels_count)
{
	if (!IsVisible())
	{
		dbg("not visible.\n");
		return;
	}

	if (widthRect > width || heightRect > height)
	{
		dbg("rect does not fit\n");
		return;
	}

	PixelNumber xabs = x;
	PixelNumber yabs = y;

	if (parent)
	{
		xabs += parent->Xpos();
		yabs += parent->Ypos();
	}

	if (widthRect < width)
	{
		xabs += (width - widthRect);
	}

	if (heightRect < height)
	{
		yabs += (height - heightRect) / 2;
	}

	lcd.drawBitmapRgbaStream(xabs, yabs, widthRect, heightRect, pixels_offset, reinterpret_cast<const uint32_t *>(pixels), pixels_count);
	changed = false;
}

void DrawDirect::DrawRect565(PixelNumber widthRect, PixelNumber heightRect, unsigned int pixels_offset, const uint16_t *pixels, size_t pixels_count)
{
	if (!IsVisible() || widthRect > width || heightRect > height)
	{
		return;
	}

	PixelNumber xabs = x;
	PixelNumber yabs = y;
	if (parent)
	{
		xabs += parent->Xpos();
		yabs += parent->Ypos();
	}
	if (widthRect < width)
	{
		xabs += (width - widthRect) / 2;
	}
	if (heightRect < height)
	{
		yabs += (height - heightRect) / 2;
	}
	lcd.drawBitmap565Stream(xabs, yabs, widthRect, heightRect, pixels_offset, pixels, pixels_count);
	changed = false;
}

// End
