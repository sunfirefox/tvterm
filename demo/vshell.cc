/*
Copyright (C) 2014 Witold Filipczyk
Copyright (C) 2009 Bryan Christ

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
This library is based on ROTE written by Bruno Takahashi C. de Oliveira
*/

#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>

#include "vterm.h"
#include "vterm_private.h"
#include "vterm_colors.h"

#define Uses_TApplication
#define Uses_TButton
#define Uses_TCommandSet
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TFrame
#define Uses_TGKey
#define Uses_TKeys
#define Uses_TKeys_Extended
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TMenuItem
#define Uses_TMenuSubItem
#define Uses_TNSCollection
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TVCodePage
#define Uses_TVOSClipboard
#include <tv.h>

int screen_w, screen_h;

unsigned char remap_char[256];
unsigned char remap_frame[256];
unsigned char remap_letter[256];

enum
{
	cmAbout	= 101,	//about box
	cmCreate,	//creates a new life window
	cmRedraw,
};

struct colors col[64];

short color_values[] = {
	0,//0
	4,//1
	2,//2
	6,//3 //6
	1,//4
	5,//5
	3,//6
	7//7
};

static bool linux_console_8859_2;

chtype acs_map[128];

static void
init_colors(void)
{
	int i, j;

	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 8; j++)
		{
			if (i != 7 || j != 0)
			{
				int ind = j * 8 + i;
				col[ind].fg = i;
				col[ind].bg = j;
			}
		}
	}
	col[0].fg = 7;
	col[0].bg = 0;
}

class TTerminalWindow;

class TWindowTerm: public TView
{
public:
	TWindowTerm(TRect& bounds, TTerminalWindow *parent);
	~TWindowTerm();
	void draw();
	void handleEvent(TEvent &event);
	void sendMouseEvent(TEvent &event);
	void tryPaste(TEvent &event, int clip);
	void zmienRozmiar(TPoint s);
	vterm_t *vterm;
	TTerminalWindow *pWindow;
};

class TTerminalWindow: public TWindow
{
public:
	static const int minW = 28;
	static const int minH = 11;
	TTerminalWindow(TRect& bounds, char* str, int num);
	~TTerminalWindow();
	void sizeLimits(TPoint& min, TPoint& max);
	void dragView( TEvent& event, uchar mode, TRect& limits, TPoint minSize, TPoint maxSize);
	void zoom();
	void setTitle(char *text);
	TWindowTerm* win;
};

class TMyApp: public TApplication
{
public:
	TMyApp();
	static TMenuBar* initMenuBar(TRect r);
	static TStatusLine* initStatusLine(TRect r);
	void aboutBox();
	void handleEvent(TEvent &event);
	void idle();
	void createTerminalWindow();
                                      // Previous callback in the code page chain
	static TVCodePageCallBack oldCPCallBack;
	static void cpCallBack(unsigned short *map); // That's our callback
};

TMyApp *app;

TVCodePageCallBack TMyApp::oldCPCallBack = NULL;

typedef unsigned char para[2];

static unsigned char remap_pairs_8859_1[][2] =
{
	{0, 0},
};

static unsigned char remap_pairs_frames_8859_1[][2] =
{
	{'j', 137},
	{'k', 140},
	{'l', 134},
	{'m', 131},
	{'n', 143},
	{'q', 138},
	{'t', 135},
	{'u', 141},
	{'v', 139},
	{'w', 142},
	{'x', 133},
	{0, 0},
};

static unsigned char remap_pairs_letters_8859_1[][2] =
{
	{0, 0},
};

static unsigned char remap_pairs_8859_2[][2] =
{
	{16, 195},
	{17, 194},
	{18, 193},
	{24, 196},
	{30, 196},
	{31, 193},
	{177, 197},
	{178, 199},
	{179, 223},
	{180, 218},
	{186, 223},
	{187, 222},
	{188, 209},
	{191, 222},
	{192, 208},
	{195, 219},
	{196, 204},
	{200, 208},
	{201, 221},
	{205, 204},
	{217, 209},
	{218, 221},
	{254, '*'},
	{0, 0},
};

static unsigned char remap_pairs_frames_8859_2[][2] =
{
	{'a', 177},
	{'j', 217}, ///209},//217},
	{'k', 191},
	{'l', 218}, ///221},//218},
	{'m', 192},
	{'n', 197},
	{'q', 196},
	{'t', 195},
	{'u', 180}, ///218},//180},
	{'v', 193},
	{'w', 194},
	{'x', 179},///223},//179},
	{0, 0},
};

static unsigned char remap_pairs_letters_8859_2[][2] =
{
	{198, 6},
	{202, 10},
	{209, 17},
	{211, 19},
	{0, 0},
};

static void init_remap_chars(void)
{
	unsigned char remap_frame_tmp[256];
	para *remap_pairs;
	para *remap_pairs_frames;
	para *remap_pairs_letters;

	for (int i = 0; i <= 255; ++i)
	{
		remap_char[i] = (unsigned char)i;
		remap_frame_tmp[i] = (unsigned char)i;
		remap_letter[i] = (unsigned char)i;
		remap_frame[i] = (unsigned char)i;
	}

	if (linux_console_8859_2)
	{
		remap_pairs = remap_pairs_8859_2;
		remap_pairs_frames = remap_pairs_frames_8859_2;
		remap_pairs_letters = remap_pairs_letters_8859_2;
	}
	else
	{
		remap_pairs = remap_pairs_8859_1;
		remap_pairs_frames = remap_pairs_frames_8859_1;
		remap_pairs_letters = remap_pairs_letters_8859_1;
	}

	for (int i = 0; remap_pairs[i][0] != remap_pairs[i][1]; ++i)
	{
		remap_char[remap_pairs[i][0]] = remap_pairs[i][1];
	}
	for (int i = 0; remap_pairs_frames[i][0] != remap_pairs_frames[i][1]; ++i)
	{
		remap_frame_tmp[remap_pairs_frames[i][0]] = remap_pairs_frames[i][1];
	}
	for (int i = 0; remap_pairs_letters[i][0] != remap_pairs_letters[i][1]; ++i)
	{
		remap_letter[remap_pairs_letters[i][0]] = remap_pairs_letters[i][1];
	}

	for (int i = 0; i <= 255; ++i)
	{
		remap_frame[i] = remap_char[remap_frame_tmp[i]];
	}
}

static unsigned char ourRemapChar(unsigned char ch, unsigned short *map)
{
	return remap_char[ch];
}

chtype tv_frames(chtype c)
{
	return remap_frame[(unsigned char)c];
}

chtype tv_capital(chtype c)
{
	return remap_letter[(unsigned char)c];
}

static void ourRemapString(unsigned char *out, unsigned char *in, unsigned short *map)
{
	for (int i = 0; in[i]; ++i)
	{
		out[i] = ourRemapChar(in[i], map);
	}
}

void TMyApp::cpCallBack(unsigned short *map)
{
	TDeskTop::defaultBkgrnd = 199;
	ourRemapString((unsigned char *)TFrame::zoomIcon, (unsigned char *)TFrame::ozoomIcon, map);
	ourRemapString((unsigned char *)TFrame::unZoomIcon, (unsigned char *)TFrame::ounZoomIcon, map);
	ourRemapString((unsigned char *)TFrame::frameChars, (unsigned char *)TFrame::oframeChars, map);
	ourRemapString((unsigned char *)TMenuBox::frameChars, (unsigned char *)TMenuBox::oframeChars, map);
	ourRemapString((unsigned char *)TFrame::dragIcon, (unsigned char *)TFrame::odragIcon, map);

	if (oldCPCallBack) oldCPCallBack(map);
}

TWindowTerm::TWindowTerm(TRect& bounds, TTerminalWindow *parent) : TView(bounds), pWindow(parent)
{
	eventMask |= evMouseUp;
	options = ofSelectable;
	vterm = vterm_create(size.x, size.y, 0);
	if (vterm)
	{
		vterm_set_colors(vterm, COLOR_WHITE, COLOR_BLACK);
	}
}

void TWindowTerm::zmienRozmiar(TPoint s)
{
	if (vterm)
	{
		vterm_resize(vterm, s.x-2, s.y-2);
	}
	growTo(s.x-2,s.y-2);
	drawView();
}

static void decodeKey(short k, chtype ch[2])
{
	ch[0] = ERR;
	ch[1] = 0;

	//fprintf(stderr, "k=%d\n", k);

	switch (k)
	{
	case kbEnter:
		//fprintf(stderr, "kbEnter\n");
		ch[0] = 10;
		break;
	case kbEsc:
		//fprintf(stderr, "ESC\n");
		ch[0] = 27;
		break;
	case kbDown:
		//fprintf(stderr, "DOWN\n");
		ch[0] = KEY_DOWN;
		break;
	case kbUp:
		//fprintf(stderr, "UP\n");
		ch[0] = KEY_UP;
		break;
	case kbLeft:
		//fprintf(stderr, "LEFT\n");
		ch[0] = KEY_LEFT;
		break;
	case kbRight:
		//fprintf(stderr, "RIGHT\n");
		ch[0] = KEY_RIGHT;
		break;
	case kbBackSpace:
		//fprintf(stderr, "BACKSPACE\n");
		ch[0] = KEY_BACKSPACE;
		break;
	case kbTab:
		ch[0] = 9;
		break;
	case kbPgDn:
		ch[0] = KEY_NPAGE;
		break;
	case kbPgUp:
		ch[0] = KEY_PPAGE;
		break;
	case kbHome:
		ch[0] = KEY_HOME;
		break;
	case kbEnd:
		ch[0] = KEY_END;
		break;
	case kbInsert:
		ch[0] = KEY_IC;
		break;
	case kbDelete:
		ch[0] = KEY_DC;
		break;
	case kbF1:
		ch[0] = KEY_F(1);
		break;
	case kbF2:
		ch[0] = KEY_F(2);
		break;
	case kbF3:
		ch[0] = KEY_F(3);
		break;
	case kbF4:
		ch[0] = KEY_F(4);
		break;
	case kbF5:
		ch[0] = KEY_F(5);
		break;
	case kbF6:
		ch[0] = KEY_F(6);
		break;
	case kbF7:
		ch[0] = KEY_F(7);
		break;
	case kbF8:
		ch[0] = KEY_F(8);
		break;
	case kbF9:
		ch[0] = KEY_F(9);
		break;
	case kbF10:
		ch[0] = KEY_F(10);
		break;
	case kbF11:
		ch[0] = KEY_F(11);
		break;
	case kbF12:
		ch[0] = KEY_F(12);
		break;
	case kbAlA:
		ch[0] = 27;
		ch[1] = 'A';
		break;
	case kbAlB:
		ch[0] = 27;
		ch[1] = 'B';
		break;
	case kbAlC:
		ch[0] = 27;
		ch[1] = 'C';
		break;
	case kbAlD:
		ch[0] = 27;
		ch[1] = 'D';
		break;
	case kbAlE:
		ch[0] = 27;
		ch[1] = 'E';
		break;
	case kbAlF:
		ch[0] = 27;
		ch[1] = 'F';
		break;
	case kbAlG:
		ch[0] = 27;
		ch[1] = 'G';
		break;
	case kbAlH:
		ch[0] = 27;
		ch[1] = 'H';
		break;
	case kbAlI:
		ch[0] = 27;
		ch[1] = 'I';
		break;
	case kbAlJ:
		ch[0] = 27;
		ch[1] = 'J';
		break;
	case kbAlK:
		ch[0] = 27;
		ch[1] = 'K';
		break;
	case kbAlL:
		ch[0] = 27;
		ch[1] = 'L';
		break;
	case kbAlM:
		ch[0] = 27;
		ch[1] = 'M';
		break;
	case kbAlN:
		ch[0] = 27;
		ch[1] = 'N';
		break;
	case kbAlO:
		ch[0] = 27;
		ch[1] = 'O';
		break;
	case kbAlP:
		ch[0] = 27;
		ch[1] = 'P';
		break;
	case kbAlQ:
		ch[0] = 27;
		ch[1] = 'Q';
		break;
	case kbAlR:
		ch[0] = 27;
		ch[1] = 'R';
		break;
	case kbAlS:
		ch[0] = 27;
		ch[1] = 'S';
		break;
	case kbAlT:
		ch[0] = 27;
		ch[1] = 'T';
		break;
	case kbAlU:
		ch[0] = 27;
		ch[1] = 'U';
		break;
	case kbAlV:
		ch[0] = 27;
		ch[1] = 'V';
		break;
	case kbAlW:
		ch[0] = 27;
		ch[1] = 'W';
		break;
	case kbAlX:
		ch[0] = 27;
		ch[1] = 'X';
		break;
	case kbAlY:
		ch[0] = 27;
		ch[1] = 'Y';
		break;
	case kbAlZ:
		ch[0] = 27;
		ch[1] = 'Z';
		break;
	case kbAl0:
		ch[0] = 27;
		ch[1] = '0';
		break;
	case kbAl1:
		ch[0] = 27;
		ch[1] = '1';
		break;
	case kbAl2:
		ch[0] = 27;
		ch[1] = '2';
		break;
	case kbAl3:
		ch[0] = 27;
		ch[1] = '3';
		break;
	case kbAl4:
		ch[0] = 27;
		ch[1] = '4';
		break;
	case kbAl5:
		ch[0] = 27;
		ch[1] = '5';
		break;
	case kbAl6:
		ch[0] = 27;
		ch[1] = '6';
		break;
	case kbAl7:
		ch[0] = 27;
		ch[1] = '7';
		break;
	case kbAl8:
		ch[0] = 27;
		ch[1] = '8';
		break;
	case kbAl9:
		ch[0] = 27;
		ch[1] = '9';
		break;
	default:
		break;
	}
}

static short reverse_color(short color)
{
	short a = (color & 15);
	short b = (color & 240);

	return (a << 4) | (b >> 4);
}

void TWindowTerm::draw()
{
	if (vterm == NULL) return;

	short my = min(size.y, vterm->rows);
	short mx = min(size.x, vterm->cols);

	for (int y = 0; y < my; ++y)
	{
		TDrawBuffer d;
		for (int x = 0; x < mx; ++x)
		{
			d.putAttribute(x, vterm->cells[y][x].attr & A_REVERSE ?
			(reverse_color(vterm->cells[y][x].color)) : vterm->cells[y][x].color);
			d.putChar(x, vterm->cells[y][x].ch);
		}
		writeLine(0, y, mx, 1, d);
	}
	setCursor(vterm->ccol, vterm->crow);
	if (vterm->state & STATE_CURSOR_INVIS)
	{
		hideCursor();
	}
	else
	{
		showCursor();
	}
	if (vterm->state & STATE_TITLE_CHANGED)
	{
		pWindow->setTitle(vterm->title);
		vterm->state &= ~STATE_TITLE_CHANGED;
		pWindow->frame->draw();
	}
}

void TWindowTerm::tryPaste(TEvent &event, int clip)
{
	if (vterm == NULL) return;

	if (clip && !(event.mouse.buttons & 2)) return;
	if (!TVOSClipboard::isAvailable()) return;
	if (clip >= TVOSClipboard::isAvailable()) return;

	unsigned length;
	char *buffer = TVOSClipboard::paste(clip, length);
	if (!buffer) return;
	::write(vterm->pty_fd, buffer, length);
}

void TWindowTerm::sendMouseEvent(TEvent &event)
{
	if (vterm == NULL) return;

	unsigned char buffer[6];
	unsigned char b = 3;
	TPoint l = makeLocal(event.mouse.where);

	if (event.mouse.buttons & 1) b = 0;
	else if (event.mouse.buttons & 2) b = 1;
	else if (event.mouse.buttons & 4) b = 2;
	else if (event.mouse.buttons & 8) b = 96-32;
	else if (event.mouse.buttons & 16) b = 97-32;
	else if ((event.mouse.buttons & 7) == 0) b = 3;

	buffer[0] = '\033';
	buffer[1] = '[';
	buffer[2] = 'M';
	buffer[3] = b + ' ';
	buffer[4] = l.x + ' ' + 1;
	buffer[5] = l.y + ' ' + 1;
	::write(vterm->pty_fd, buffer, 6);
}

void TWindowTerm::handleEvent(TEvent& event)
{
	if (vterm == NULL) return;

	chtype ch[2] = {0};

	TView::handleEvent(event);
	switch (event.what)
	{
	case evKeyDown:
		ch[0] = event.keyDown.charScan.charCode;
		//fprintf(stderr, "evKeyDown ch=%d\n", ch);
		if (ch[0] == ERR || ch[0] == 0)
		{
			decodeKey(event.keyDown.keyCode, ch);
		}
		if (ch[0] != ERR) vterm_write_pipe(vterm, ch[0]);
		if (ch[1]) vterm_write_pipe(vterm, ch[1]);
		clearEvent(event);
		break;
	case evMouseDown:
	case evMouseUp:
		if (vterm->state & STATE_MOUSE)
		{
			sendMouseEvent(event);
		}
		if (!(vterm->state & STATE_MOUSE) || (TGKey::getShiftState() & (kbLeftShiftDown|kbRightShiftDown)))
		{
			tryPaste(event, 1);
		}
		clearEvent(event);
		break;
	case evCommand:
		if (event.message.command == cmPaste)
		{
			tryPaste(event, 0);
			clearEvent(event);
		}
		break;
	default:
		break;
	}
}

TNSCollection terminale(10, 1);

TWindowTerm::~TWindowTerm()
{
	//fprintf(stderr, "~TwindowTerm\n");
	terminale.remove(this);
	if (vterm) vterm_destroy(vterm);
}


TTerminalWindow::TTerminalWindow(TRect& bounds, char* str, int num):
	TWindowInit(&TWindow::initFrame), TWindow(bounds, str, num)
{
//	options |= ofFirstClick | ofTileable;
	eventMask = evMouseDown | evMouseUp | evKeyDown | evCommand | evBroadcast;

	TRect r = getClipRect();
	r.grow(-1, -1);
	win = new TWindowTerm(r, this);
	if (win)
	{
		insert(win);
		terminale.insert(win);
	}
}

TTerminalWindow::~TTerminalWindow()
{
	app->statusLine->draw();
}

void TTerminalWindow::setTitle(char *text)
{
	DeleteArray((char *)title);
	TVIntl::freeSt(intlTitle);
	title = newStr(text);
}

void TTerminalWindow::dragView(TEvent& event, uchar mode, TRect& limits, TPoint minSize, TPoint maxSize)
{
	TWindow::dragView(event, mode, limits, minSize, maxSize);
	win->zmienRozmiar(size);
}

void TTerminalWindow::zoom()
{
	TWindow::zoom();
	win->zmienRozmiar(size);
}

void TTerminalWindow::sizeLimits(TPoint &min, TPoint &max)
{
	TView::sizeLimits(min, max);
	min.x = minW;
	min.y = minH;
}

TMyApp::TMyApp(): TProgInit(&TMyApp::initStatusLine, &TMyApp::initMenuBar,
	&TMyApp::initDeskTop)/*, windowCommands(getCommands()*)*/
{
}

void TMyApp::handleEvent(TEvent &event)
{
	TApplication::handleEvent(event);
	if (event.what == evCommand)
	{
		switch (event.message.command)
		{
		case cmCreate:
			createTerminalWindow();
			statusLine->draw();
			break;
		case cmNext:
			selectNext(true);
			break;
		case cmPrev:
			selectNext(false);
			break;
		case cmRedraw:
			menuBar->draw();
			deskTop->draw();
			statusLine->draw();
			break;
		default:
			return;
		}
		clearEvent(event);
	}
}

void TMyApp::createTerminalWindow()
{
	TRect r(0, 0, 75, 20);
	r.grow(-1, -1);

	TTerminalWindow *w = new TTerminalWindow(r, (char *)"tvterm", 0);
	if (w)
	{
		deskTop->insert(w);
	}
}

void loop1(void *v, void *arg)
{
	TWindowTerm *tw = (TWindowTerm *)v;
	if (!tw || !tw->vterm) return;
	int bytes = vterm_read_pipe(tw->vterm);
	if (bytes > 0)
	{
		tw->drawView();
	}
	else if (bytes == -1)
	{
		tw->pWindow->close();
	}
}

void TMyApp::idle()
{
	terminale.forEach(loop1, NULL);
}

ushort executeDialog( TDialog* pD, void* data=0 )
{
	ushort c = cmCancel;

	if (TProgram::application->validView(pD))
	{
		if (data) pD->setData(data);
		c = TProgram::deskTop->execView(pD);
		if ((c != cmCancel) && (data))
			pD->getData(data);
		CLY_destroy(pD);
	}
	return c;
}

TStatusLine* TMyApp::initStatusLine(TRect r)
{
	r.a.y = r.b.y - 1;
	return new TStatusLine(r,
	*new TStatusDef(0, 90) +
	*new TStatusItem("~Alt-X~ Exit", kbAlX, cmQuit) +
	*new TStatusItem("~Alt-N~ New terminal", kbAlN, cmCreate) +
	*new TStatusItem("~Alt-F3~ Close", kbAlF3, cmClose) +
	*new TStatusItem("~F11~ Redraw", kbF11, cmRedraw) +
	*new TStatusItem("~F12~ Zoom", kbF12, cmZoom) +
	*new TStatusItem(0, kbCtF10, cmMenu) +
	*new TStatusItem(0, kbAlTab, cmNext) +
	*new TStatusItem(0, kbCtAlTab, cmPrev) +
	*new TStatusItem(0, kbShInsert, cmPaste) +
	*new TStatusItem(0, kbCtF5, cmResize));
}

TMenuBar* TMyApp::initMenuBar(TRect r)
{
	TSubMenu& sub1 = *new TSubMenu("~F~ile", 0) +
	*new TMenuItem("New ~t~erminal", cmCreate, kbAlN, hcNoContext, "Alt-N") +
	newLine() +
	*new TMenuItem("E~x~it", cmQuit, kbAlX, hcNoContext, "Alt-X");

	r.b.y =  r.a.y + 1;
	return new TMenuBar(r, sub1);
}

int main()
{
	char *term = getenv("TERM");
	setlocale(LC_ALL,"");

	if (term && !strcmp(term, "linux"))
	{
		char *codeset = nl_langinfo(CODESET);

		if (codeset && !strcmp(codeset, "ISO-8859-2"))
		{
			linux_console_8859_2 = true;
		}
	} 
	init_colors();
	init_remap_chars();

	if (linux_console_8859_2) TMyApp::oldCPCallBack = TVCodePage::SetCallBack(TMyApp::cpCallBack);

	app = new TMyApp();
	if (app)
	{
		app->run();
		delete app;
	}
	return 0;
}
