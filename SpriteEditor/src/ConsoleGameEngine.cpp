#include "ConsoleGameEngine.h"

ConsoleGameEngine::ConsoleGameEngine() {
	m_nScreenWidth = 80;
	m_nScreenHeight = 30;

	m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	m_hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

	std::memset(m_keyNewState, 0, sizeof(m_keyNewState));
	std::memset(m_keyOldState, 0, sizeof(m_keyOldState));
	std::memset(m_mouseNewState, 0, sizeof(m_mouseOldState));
	std::memset(m_mouseOldState, 0, sizeof(m_mouseOldState));
	std::memset(m_keys, 0, 256 * sizeof(sKeyState));
	std::memset(m_mouse, 0, 5 * sizeof(sKeyState));

	m_mousePosX = 0;
	m_mousePosY = 0;

	m_bConsoleInFocus = true;
	m_bEnableSound = false;
	m_hideFPS = false;

	m_sAppName = L"Default";
}

void ConsoleGameEngine::EnableSound() {
	m_bEnableSound = true;
}

void ConsoleGameEngine::HideFPS(){
	m_hideFPS = true;
}

int ConsoleGameEngine::ConstructConsole(int width, int height, int fontw, int fonth) {
	if (m_hConsole == INVALID_HANDLE_VALUE)
		return Error(L"Bad Handle");

	m_nScreenWidth = width;
	m_nScreenHeight = height;

	// Update 13/09/2017 - It seems that the console behaves differently on some systems
	// and I'm unsure why this is. It could be to do with windows default settings, or
	// screen resolutions, or system languages. Unfortunately, MSDN does not offer much
	// by way of useful information, and so the resulting sequence is the reult of experiment
	// that seems to work in multiple cases.
	//
	// The problem seems to be that the SetConsoleXXX functions are somewhat circular and 
	// fail depending on the state of the current console properties, i.e. you can't set
	// the buffer size until you set the screen size, but you can't change the screen size
	// until the buffer size is correct. This coupled with a precise ordering of calls
	// makes this procedure seem a little mystical :-P. Thanks to wowLinh for helping - Jx9

	// Change console visual size to a minimum so ScreenBuffer can shrink
	// below the actual visual size
	m_rectWindow.Left = 0;
	m_rectWindow.Top = 0;
	m_rectWindow.Right = 1;
	m_rectWindow.Bottom = 1;

	SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow);

	// Set the size of the screen buffer
	COORD coord = {(short) m_nScreenWidth, (short) m_nScreenHeight};
	if (!SetConsoleScreenBufferSize(m_hConsole, coord))
		Error(L"SetConsoleScreenBufferSize");

	// Assign screen buffer to the console
	if (!SetConsoleActiveScreenBuffer(m_hConsole))
		return Error(L"SetConsoleActiveScreenBuffer");

	// Set the font size now that the screen buffer has been assigned to the console
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = fontw;
	cfi.dwFontSize.Y = fonth;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;

	/*	DWORD version = GetVersion();
		DWORD major = (DWORD)(LOBYTE(LOWORD(version)));
		DWORD minor = (DWORD)(HIBYTE(LOWORD(version)));*/

		//if ((major > 6) || ((major == 6) && (minor >= 2) && (minor < 4)))		
		//	wcscpy_s(cfi.FaceName, L"Raster"); // Windows 8 :(
		//else
		//	wcscpy_s(cfi.FaceName, L"Lucida Console"); // Everything else :P

		//wcscpy_s(cfi.FaceName, L"Liberation Mono");
	wcscpy_s(cfi.FaceName, L"Consolas");
	if (!SetCurrentConsoleFontEx(m_hConsole, false, &cfi))
		return Error(L"SetCurrentConsoleFontEx");

	// Get screen buffer info and check the maximum allowed window size. Return
	// error if exceeded, so user knows their dimensions/fontsize are too large
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(m_hConsole, &csbi))
		return Error(L"GetConsoleScreenBufferInfo");
	if (m_nScreenHeight > csbi.dwMaximumWindowSize.Y)
		return Error(L"Screen Height / Font Height Too Big");
	if (m_nScreenWidth > csbi.dwMaximumWindowSize.X)
		return Error(L"Screen Width / Font Width Too Big");

	// Set Physical Console Window Size
	m_rectWindow.Left = 0;
	m_rectWindow.Top = 0;
	m_rectWindow.Right = (short) m_nScreenWidth - 1;
	m_rectWindow.Bottom = (short) m_nScreenHeight - 1;

	if (!SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow))
		return Error(L"SetConsoleWindowInfo");

	// Set flags to allow mouse input		
	if (!SetConsoleMode(m_hConsoleIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT))
		return Error(L"SetConsoleMode");

	// Allocate memory for screen buffer
	m_bufScreen = new CHAR_INFO[m_nScreenWidth * m_nScreenHeight];
	memset(m_bufScreen, 0, sizeof(CHAR_INFO) * m_nScreenWidth * m_nScreenHeight);

	SetConsoleCtrlHandler((PHANDLER_ROUTINE) CloseHandler, TRUE);
	return 1;
}

void ConsoleGameEngine::Draw(int x, int y, short c, short col) {
	if (x >= 0 && x < m_nScreenWidth && y >= 0 && y < m_nScreenHeight) {
		m_bufScreen[y * m_nScreenWidth + x].Char.UnicodeChar = c;
		m_bufScreen[y * m_nScreenWidth + x].Attributes = col;
	}
}

void ConsoleGameEngine::Fill(int x1, int y1, int x2, int y2, short c, short col) {
	Clip(x1, y1);
	Clip(x2, y2);
	for (int x = x1; x < x2; x++)
		for (int y = y1; y < y2; y++)
			Draw(x, y, c, col);
}

void ConsoleGameEngine::DrawString(int x, int y, std::wstring c, short col) {
	for (size_t i = 0; i < c.size(); i++) {
		m_bufScreen[y * m_nScreenWidth + x + i].Char.UnicodeChar = c[i];
		m_bufScreen[y * m_nScreenWidth + x + i].Attributes = col;
	}
}

void ConsoleGameEngine::DrawStringAlpha(int x, int y, std::wstring c, short col) {
	for (size_t i = 0; i < c.size(); i++) {
		if (c[i] != L' ') {
			m_bufScreen[y * m_nScreenWidth + x + i].Char.UnicodeChar = c[i];
			m_bufScreen[y * m_nScreenWidth + x + i].Attributes = col;
		}
	}
}

void ConsoleGameEngine::Clip(int& x, int& y) {
	if (x < 0) x = 0;
	if (x >= m_nScreenWidth) x = m_nScreenWidth;
	if (y < 0) y = 0;
	if (y >= m_nScreenHeight) y = m_nScreenHeight;
}

void ConsoleGameEngine::DrawLine(int x1, int y1, int x2, int y2, short c, short col) {
	int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
	dx = x2 - x1; dy = y2 - y1;
	dx1 = abs(dx); dy1 = abs(dy);
	px = 2 * dy1 - dx1;	py = 2 * dx1 - dy1;
	if (dy1 <= dx1) {
		if (dx >= 0) {
			x = x1; y = y1; xe = x2;
		} else {
			x = x2; y = y2; xe = x1;
		}

		Draw(x, y, c, col);

		for (i = 0; x < xe; i++) {
			x = x + 1;
			if (px < 0)
				px = px + 2 * dy1;
			else {
				if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) y = y + 1; else y = y - 1;
				px = px + 2 * (dy1 - dx1);
			}
			Draw(x, y, c, col);
		}
	} else {
		if (dy >= 0) {
			x = x1; y = y1; ye = y2;
		} else {
			x = x2; y = y2; ye = y1;
		}

		Draw(x, y, c, col);

		for (i = 0; y < ye; i++) {
			y = y + 1;
			if (py <= 0)
				py = py + 2 * dx1;
			else {
				if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) x = x + 1; else x = x - 1;
				py = py + 2 * (dx1 - dy1);
			}
			Draw(x, y, c, col);
		}
	}
}

void ConsoleGameEngine::DrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, short c, short col) {
	DrawLine(x1, y1, x2, y2, c, col);
	DrawLine(x2, y2, x3, y3, c, col);
	DrawLine(x3, y3, x1, y1, c, col);
}

void ConsoleGameEngine::FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, short c, short col) {
	auto SWAP = [] (int& x, int& y) { int t = x; x = y; y = t; };
	auto drawline = [&] (int sx, int ex, int ny) { for (int i = sx; i <= ex; i++) Draw(i, ny, c, col); };

	int t1x, t2x, y, minx, maxx, t1xp, t2xp;
	bool changed1 = false;
	bool changed2 = false;
	int signx1, signx2, dx1, dy1, dx2, dy2;
	int e1, e2;
	// Sort vertices
	if (y1 > y2) { SWAP(y1, y2); SWAP(x1, x2); }
	if (y1 > y3) { SWAP(y1, y3); SWAP(x1, x3); }
	if (y2 > y3) { SWAP(y2, y3); SWAP(x2, x3); }

	t1x = t2x = x1; y = y1;   // Starting points
	dx1 = (int) (x2 - x1); if (dx1 < 0) { dx1 = -dx1; signx1 = -1; } else signx1 = 1;
	dy1 = (int) (y2 - y1);

	dx2 = (int) (x3 - x1); if (dx2 < 0) { dx2 = -dx2; signx2 = -1; } else signx2 = 1;
	dy2 = (int) (y3 - y1);

	if (dy1 > dx1) {   // swap values
		SWAP(dx1, dy1);
		changed1 = true;
	}
	if (dy2 > dx2) {   // swap values
		SWAP(dy2, dx2);
		changed2 = true;
	}

	e2 = (int) (dx2 >> 1);
	// Flat top, just process the second half
	if (y1 == y2) goto next;
	e1 = (int) (dx1 >> 1);

	for (int i = 0; i < dx1;) {
		t1xp = 0; t2xp = 0;
		if (t1x < t2x) { minx = t1x; maxx = t2x; } else { minx = t2x; maxx = t1x; }
		// process first line until y value is about to change
		while (i < dx1) {
			i++;
			e1 += dy1;
			while (e1 >= dx1) {
				e1 -= dx1;
				if (changed1) t1xp = signx1;//t1x += signx1;
				else          goto next1;
			}
			if (changed1) break;
			else t1x += signx1;
		}
		// Move line
	next1:
		// process second line until y value is about to change
		while (1) {
			e2 += dy2;
			while (e2 >= dx2) {
				e2 -= dx2;
				if (changed2) t2xp = signx2;//t2x += signx2;
				else          goto next2;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}
	next2:
		if (minx > t1x) minx = t1x; if (minx > t2x) minx = t2x;
		if (maxx < t1x) maxx = t1x; if (maxx < t2x) maxx = t2x;
		drawline(minx, maxx, y);    // Draw line from min to max points found on the y
		// Now increase y
		if (!changed1) t1x += signx1;
		t1x += t1xp;
		if (!changed2) t2x += signx2;
		t2x += t2xp;
		y += 1;
		if (y == y2) break;

	}
next:
	// Second half
	dx1 = (int) (x3 - x2); if (dx1 < 0) { dx1 = -dx1; signx1 = -1; } else signx1 = 1;
	dy1 = (int) (y3 - y2);
	t1x = x2;

	if (dy1 > dx1) {   // swap values
		SWAP(dy1, dx1);
		changed1 = true;
	} else changed1 = false;

	e1 = (int) (dx1 >> 1);

	for (int i = 0; i <= dx1; i++) {
		t1xp = 0; t2xp = 0;
		if (t1x < t2x) { minx = t1x; maxx = t2x; } else { minx = t2x; maxx = t1x; }
		// process first line until y value is about to change
		while (i < dx1) {
			e1 += dy1;
			while (e1 >= dx1) {
				e1 -= dx1;
				if (changed1) { t1xp = signx1; break; }//t1x += signx1;
				else          goto next3;
			}
			if (changed1) break;
			else   	   	  t1x += signx1;
			if (i < dx1) i++;
		}
	next3:
		// process second line until y value is about to change
		while (t2x != x3) {
			e2 += dy2;
			while (e2 >= dx2) {
				e2 -= dx2;
				if (changed2) t2xp = signx2;
				else          goto next4;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}
	next4:

		if (minx > t1x) minx = t1x; if (minx > t2x) minx = t2x;
		if (maxx < t1x) maxx = t1x; if (maxx < t2x) maxx = t2x;
		drawline(minx, maxx, y);
		if (!changed1) t1x += signx1;
		t1x += t1xp;
		if (!changed2) t2x += signx2;
		t2x += t2xp;
		y += 1;
		if (y > y3) return;
	}
}

void ConsoleGameEngine::DrawCircle(int xc, int yc, int r, short c, short col) {
	int x = 0;
	int y = r;
	int p = 3 - 2 * r;
	if (!r) return;

	while (y >= x) // only formulate 1/8 of circle
	{
		Draw(xc - x, yc - y, c, col);//upper left left
		Draw(xc - y, yc - x, c, col);//upper upper left
		Draw(xc + y, yc - x, c, col);//upper upper right
		Draw(xc + x, yc - y, c, col);//upper right right
		Draw(xc - x, yc + y, c, col);//lower left left
		Draw(xc - y, yc + x, c, col);//lower lower left
		Draw(xc + y, yc + x, c, col);//lower lower right
		Draw(xc + x, yc + y, c, col);//lower right right
		if (p < 0) p += 4 * x++ + 6;
		else p += 4 * (x++ - y--) + 10;
	}
}

void ConsoleGameEngine::FillCircle(int xc, int yc, int r, short c, short col) {
	// Taken from wikipedia
	int x = 0;
	int y = r;
	int p = 3 - 2 * r;
	if (!r) return;

	auto drawline = [&] (int sx, int ex, int ny) {
		for (int i = sx; i <= ex; i++)
			Draw(i, ny, c, col);
		};

	while (y >= x) {
		// Modified to draw scan-lines instead of edges
		drawline(xc - x, xc + x, yc - y);
		drawline(xc - y, xc + y, yc - x);
		drawline(xc - x, xc + x, yc + y);
		drawline(xc - y, xc + y, yc + x);
		if (p < 0) p += 4 * x++ + 6;
		else p += 4 * (x++ - y--) + 10;
	}
};

void ConsoleGameEngine::DrawSprite(int x, int y, Sprite* sprite) {
	if (sprite == nullptr)
		return;

	for (int i = 0; i < sprite->nWidth; i++) {
		for (int j = 0; j < sprite->nHeight; j++) {
			if (sprite->GetGlyph(i, j) != L' ')
				Draw(x + i, y + j, sprite->GetGlyph(i, j), sprite->GetColour(i, j));
		}
	}
}

void ConsoleGameEngine::DrawPartialSprite(int x, int y, Sprite* sprite, int ox, int oy, int w, int h) {
	if (sprite == nullptr)
		return;

	for (int i = 0; i < w; i++) {
		for (int j = 0; j < h; j++) {
			if (sprite->GetGlyph(i + ox, j + oy) != L' ')
				Draw(x + i, y + j, sprite->GetGlyph(i + ox, j + oy), sprite->GetColour(i + ox, j + oy));
		}
	}
}

void ConsoleGameEngine::DrawWireFrameModel(const std::vector<std::pair<float, float>>& vecModelCoordinates, float x, float y, float r, float s, short col, short c) {
	// pair.first = x coordinate
	// pair.second = y coordinate

	// Create translated model vector of coordinate pairs
	std::vector<std::pair<float, float>> vecTransformedCoordinates;
	int verts = vecModelCoordinates.size();
	vecTransformedCoordinates.resize(verts);

	// Rotate
	for (int i = 0; i < verts; i++) {
		vecTransformedCoordinates[i].first = vecModelCoordinates[i].first * cosf(r) - vecModelCoordinates[i].second * sinf(r);
		vecTransformedCoordinates[i].second = vecModelCoordinates[i].first * sinf(r) + vecModelCoordinates[i].second * cosf(r);
	}

	// Scale
	for (int i = 0; i < verts; i++) {
		vecTransformedCoordinates[i].first = vecTransformedCoordinates[i].first * s;
		vecTransformedCoordinates[i].second = vecTransformedCoordinates[i].second * s;
	}

	// Translate
	for (int i = 0; i < verts; i++) {
		vecTransformedCoordinates[i].first = vecTransformedCoordinates[i].first + x;
		vecTransformedCoordinates[i].second = vecTransformedCoordinates[i].second + y;
	}

	// Draw Closed Polygon
	for (int i = 0; i < verts + 1; i++) {
		int j = (i + 1);
		DrawLine((int) vecTransformedCoordinates[i % verts].first, (int) vecTransformedCoordinates[i % verts].second,
				 (int) vecTransformedCoordinates[j % verts].first, (int) vecTransformedCoordinates[j % verts].second, c, col);
	}
}

ConsoleGameEngine::~ConsoleGameEngine() {
	SetConsoleActiveScreenBuffer(m_hOriginalConsole);
	delete[] m_bufScreen;
}

void ConsoleGameEngine::Start() {
	// Start the thread
	m_bAtomActive = true;
	std::thread t = std::thread(&ConsoleGameEngine::GameThread, this);

	// Wait for thread to be exited
	t.join();
}

int ConsoleGameEngine::ScreenWidth() {
	return m_nScreenWidth;
}

int ConsoleGameEngine::ScreenHeight() {
	return m_nScreenHeight;
}

void ConsoleGameEngine::GameThread() {
	// Create user resources as part of this thread
	if (!OnUserCreate())
		m_bAtomActive = false;

	// Check if sound system should be enabled
	if (m_bEnableSound) {
		if (!CreateAudio()) {
			m_bAtomActive = false; // Failed to create audio system			
			m_bEnableSound = false;
		}
	}

	auto tp1 = std::chrono::system_clock::now();
	auto tp2 = std::chrono::system_clock::now();

	while (m_bAtomActive) {
		// Run as fast as possible
		while (m_bAtomActive) {
			// Handle Timing
			tp2 = std::chrono::system_clock::now();
			std::chrono::duration<float> elapsedTime = tp2 - tp1;
			tp1 = tp2;
			float fElapsedTime = elapsedTime.count();

			// Handle Keyboard Input
			for (int i = 0; i < 256; i++) {
				m_keyNewState[i] = GetAsyncKeyState(i);

				m_keys[i].bPressed = false;
				m_keys[i].bReleased = false;

				if (m_keyNewState[i] != m_keyOldState[i]) {
					if (m_keyNewState[i] & 0x8000) {
						m_keys[i].bPressed = !m_keys[i].bHeld;
						m_keys[i].bHeld = true;
					} else {
						m_keys[i].bReleased = true;
						m_keys[i].bHeld = false;
					}
				}

				m_keyOldState[i] = m_keyNewState[i];
			}

			// Handle Mouse Input - Check for window events
			INPUT_RECORD inBuf[32];
			DWORD events = 0;
			GetNumberOfConsoleInputEvents(m_hConsoleIn, &events);
			if (events > 0)
				ReadConsoleInput(m_hConsoleIn, inBuf, events, &events);

			// Handle events - we only care about mouse clicks and movement
			// for now
			for (DWORD i = 0; i < events; i++) {
				switch (inBuf[i].EventType) {
					case FOCUS_EVENT:
					{
						m_bConsoleInFocus = inBuf[i].Event.FocusEvent.bSetFocus;
					}
					break;

					case MOUSE_EVENT:
					{
						switch (inBuf[i].Event.MouseEvent.dwEventFlags) {
							case MOUSE_MOVED:
							{
								m_mousePosX = inBuf[i].Event.MouseEvent.dwMousePosition.X;
								m_mousePosY = inBuf[i].Event.MouseEvent.dwMousePosition.Y;
							}
							break;

							case 0:
							{
								for (int m = 0; m < 5; m++)
									m_mouseNewState[m] = (inBuf[i].Event.MouseEvent.dwButtonState & (1 << m)) > 0;

							}
							break;

							default:
								break;
						}
					}
					break;

					default:
						break;
						// We don't care just at the moment
				}
			}

			for (int m = 0; m < 5; m++) {
				m_mouse[m].bPressed = false;
				m_mouse[m].bReleased = false;

				if (m_mouseNewState[m] != m_mouseOldState[m]) {
					if (m_mouseNewState[m]) {
						m_mouse[m].bPressed = true;
						m_mouse[m].bHeld = true;
					} else {
						m_mouse[m].bReleased = true;
						m_mouse[m].bHeld = false;
					}
				}

				m_mouseOldState[m] = m_mouseNewState[m];
			}


			// Handle Frame Update
			if (!OnUserUpdate(fElapsedTime))
				m_bAtomActive = false;

			// Update Title & Present Screen Buffer
			wchar_t s[256];
			if(m_hideFPS)
				swprintf_s(s, 256, L"%s", m_sAppName.c_str());
			else
				swprintf_s(s, 256, L"%s - FPS: %3.2f", m_sAppName.c_str(), 1.0f / fElapsedTime);
			SetConsoleTitle(s);
			COORD bufferCoord;
			COORD bufferSize;
			bufferSize.X = (short) m_nScreenWidth;
			bufferSize.Y = (short) m_nScreenHeight;
			bufferCoord.X = 0;
			bufferCoord.Y = 0;
			WriteConsoleOutput(m_hConsole, m_bufScreen, bufferSize, bufferCoord, &m_rectWindow);
		}

		if (m_bEnableSound) {
			// Close and Clean up audio system
		}

		// Allow the user to free resources if they have overrided the destroy function
		if (OnUserDestroy()) {
			// User has permitted destroy, so exit and clean up
			delete[] m_bufScreen;
			SetConsoleActiveScreenBuffer(m_hOriginalConsole);
			m_cvGameFinished.notify_one();
		} else {
			// User denied destroy for some reason, so continue running
			m_bAtomActive = true;
		}
	}
}

bool ConsoleGameEngine::OnUserDestroy() {
	return true;
}

// Audio Engine =====================================================================

ConsoleGameEngine::AudioSample::AudioSample() {
	fSample = nullptr;
	nSamples = 0;
	nChannels = 0;
	bSampleValid = false;
}

ConsoleGameEngine::AudioSample::AudioSample(std::wstring sWavFile) {
	// Load Wav file and convert to float format
	FILE* f = nullptr;
	_wfopen_s(&f, sWavFile.c_str(), L"rb");
	if (f == nullptr)
		return;

	char dump[4];
	std::fread(&dump, sizeof(char), 4, f); // Read "RIFF"
	if (strncmp(dump, "RIFF", 4) != 0) return;
	std::fread(&dump, sizeof(char), 4, f); // Not Interested
	std::fread(&dump, sizeof(char), 4, f); // Read "WAVE"
	if (strncmp(dump, "WAVE", 4) != 0) return;

	// Read Wave description chunk
	std::fread(&dump, sizeof(char), 4, f); // Read "fmt "
	std::fread(&dump, sizeof(char), 4, f); // Not Interested
	std::fread(&wavHeader, sizeof(WAVEFORMATEX) - 2, 1, f); // Read Wave Format Structure chunk
	// Note the -2, because the structure has 2 bytes to indicate its own size
	// which are not in the wav file

// Just check if wave format is compatible with olcCGE
	if (wavHeader.wBitsPerSample != 16 || wavHeader.nSamplesPerSec != 44100) {
		std::fclose(f);
		return;
	}

	// Search for audio data chunk
	long nChunksize = 0;
	std::fread(&dump, sizeof(char), 4, f); // Read chunk header
	std::fread(&nChunksize, sizeof(long), 1, f); // Read chunk size
	while (strncmp(dump, "data", 4) != 0) {
		// Not audio data, so just skip it
		std::fseek(f, nChunksize, SEEK_CUR);
		std::fread(&dump, sizeof(char), 4, f);
		std::fread(&nChunksize, sizeof(long), 1, f);
	}

	// Finally got to data, so read it all in and convert to float samples
	nSamples = nChunksize / (wavHeader.nChannels * (wavHeader.wBitsPerSample >> 3));
	nChannels = wavHeader.nChannels;

	// Create floating point buffer to hold audio sample
	fSample = new float[nSamples * nChannels];
	float* pSample = fSample;

	// Read in audio data and normalise
	for (long i = 0; i < nSamples; i++) {
		for (int c = 0; c < nChannels; c++) {
			short s = 0;
			std::fread(&s, sizeof(short), 1, f);
			*pSample = (float) s / (float) (MAXSHORT);
			pSample++;
		}
	}

	// All done, flag sound as valid
	std::fclose(f);
	bSampleValid = true;
}

unsigned int ConsoleGameEngine::LoadAudioSample(std::wstring sWavFile) {
	if (!m_bEnableSound)
		return -1;

	AudioSample a(sWavFile);
	if (a.bSampleValid) {
		vecAudioSamples.push_back(a);
		return vecAudioSamples.size();
	} else
		return -1;
}

// Add sample 'id' to the mixers sounds to play list
void ConsoleGameEngine::PlaySample(int id, bool bLoop) {
	sCurrentlyPlayingSample a;
	a.nAudioSampleID = id;
	a.nSamplePosition = 0;
	a.bFinished = false;
	a.bLoop = bLoop;
	listActiveSamples.push_back(a);
}

void ConsoleGameEngine::StopSample(int id) {

}

// The audio system uses by default a specific wave format
bool ConsoleGameEngine::CreateAudio(unsigned int nSampleRate, unsigned int nChannels, unsigned int nBlocks, unsigned int nBlockSamples) {
	// Initialise Sound Engine
	m_bAudioThreadActive = false;
	m_nSampleRate = nSampleRate;
	m_nChannels = nChannels;
	m_nBlockCount = nBlocks;
	m_nBlockSamples = nBlockSamples;
	m_nBlockFree = m_nBlockCount;
	m_nBlockCurrent = 0;
	m_pBlockMemory = nullptr;
	m_pWaveHeaders = nullptr;

	// Device is available
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = m_nSampleRate;
	waveFormat.wBitsPerSample = sizeof(short) * 8;
	waveFormat.nChannels = m_nChannels;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	// Open Device if valid
	if (waveOutOpen(&m_hwDevice, WAVE_MAPPER, &waveFormat, (DWORD_PTR) waveOutProcWrap, (DWORD_PTR) this, CALLBACK_FUNCTION) != S_OK)
		return DestroyAudio();

	// Allocate Wave|Block Memory
	m_pBlockMemory = new short[m_nBlockCount * m_nBlockSamples];
	if (m_pBlockMemory == nullptr)
		return DestroyAudio();
	ZeroMemory(m_pBlockMemory, sizeof(short) * m_nBlockCount * m_nBlockSamples);

	m_pWaveHeaders = new WAVEHDR[m_nBlockCount];
	if (m_pWaveHeaders == nullptr)
		return DestroyAudio();
	ZeroMemory(m_pWaveHeaders, sizeof(WAVEHDR) * m_nBlockCount);

	// Link headers to block memory
	for (unsigned int n = 0; n < m_nBlockCount; n++) {
		m_pWaveHeaders[n].dwBufferLength = m_nBlockSamples * sizeof(short);
		m_pWaveHeaders[n].lpData = (LPSTR) (m_pBlockMemory + (n * m_nBlockSamples));
	}

	m_bAudioThreadActive = true;
	m_AudioThread = std::thread(&ConsoleGameEngine::AudioThread, this);

	// Start the ball rolling with the sound delivery thread
	std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
	m_cvBlockNotZero.notify_one();
	return true;
}

// Stop and clean up audio system
bool ConsoleGameEngine::DestroyAudio() {
	m_bAudioThreadActive = false;
	return false;
}

// Handler for soundcard request for more data
void ConsoleGameEngine::waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2) {
	if (uMsg != WOM_DONE) return;
	m_nBlockFree++;
	std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
	m_cvBlockNotZero.notify_one();
}

// Static wrapper for sound card handler
void CALLBACK ConsoleGameEngine::waveOutProcWrap(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
	((ConsoleGameEngine*) dwInstance)->waveOutProc(hWaveOut, uMsg, dwParam1, dwParam2);
}

// Audio thread. This loop responds to requests from the soundcard to fill 'blocks'
// with audio data. If no requests are available it goes dormant until the sound
// card is ready for more data. The block is fille by the "user" in some manner
// and then issued to the soundcard.
void ConsoleGameEngine::AudioThread() {
	m_fGlobalTime = 0.0f;
	float fTimeStep = 1.0f / (float) m_nSampleRate;

	// Goofy hack to get maximum integer for a type at run-time
	short nMaxSample = (short) pow(2, (sizeof(short) * 8) - 1) - 1;
	float fMaxSample = (float) nMaxSample;
	short nPreviousSample = 0;

	while (m_bAudioThreadActive) {
		// Wait for block to become available
		if (m_nBlockFree == 0) {
			std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
			while (m_nBlockFree == 0) // sometimes, Windows signals incorrectly
				m_cvBlockNotZero.wait(lm);
		}

		// Block is here, so use it
		m_nBlockFree--;

		// Prepare block for processing
		if (m_pWaveHeaders[m_nBlockCurrent].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));

		short nNewSample = 0;
		int nCurrentBlock = m_nBlockCurrent * m_nBlockSamples;

		auto clip = [] (float fSample, float fMax) {
			if (fSample >= 0.0)
				return max(fSample, fMax);
			else
				return min(fSample, -fMax);
			};

		for (unsigned int n = 0; n < m_nBlockSamples; n += m_nChannels) {
			// User Process
			for (unsigned int c = 0; c < m_nChannels; c++) {
				nNewSample = (short) (clip(GetMixerOutput(c, m_fGlobalTime, fTimeStep), 1.0) * fMaxSample);
				m_pBlockMemory[nCurrentBlock + n + c] = nNewSample;
				nPreviousSample = nNewSample;
			}

			m_fGlobalTime = m_fGlobalTime + fTimeStep;
		}

		// Send block to sound device
		waveOutPrepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
		waveOutWrite(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
		m_nBlockCurrent++;
		m_nBlockCurrent %= m_nBlockCount;
	}
}

// Overridden by user if they want to generate sound in real-time
float ConsoleGameEngine::onUserSoundSample(int nChannel, float fGlobalTime, float fTimeStep) {
	return 0.0f;
}

// Overriden by user if they want to manipulate the sound before it is played
float ConsoleGameEngine::onUserSoundFilter(int nChannel, float fGlobalTime, float fSample) {
	return fSample;
}

// The Sound Mixer - If the user wants to play many sounds simultaneously, and
// perhaps the same sound overlapping itself, then you need a mixer, which
// takes input from all sound sources for that audio frame. This mixer maintains
// a list of sound locations for all concurrently playing audio samples. Instead
// of duplicating audio data, we simply store the fact that a sound sample is in
// use and an offset into its sample data. As time progresses we update this offset
// until it is beyound the length of the sound sample it is attached to. At this
// point we remove the playing souind from the list.
//
// Additionally, the users application may want to generate sound instead of just
// playing audio clips (think a synthesizer for example) in whcih case we also
// provide an "onUser..." event to allow the user to return a sound for that point
// in time.
//
// Finally, before the sound is issued to the operating system for performing, the
// user gets one final chance to "filter" the sound, perhaps changing the volume
// or adding funky effects
float ConsoleGameEngine::GetMixerOutput(int nChannel, float fGlobalTime, float fTimeStep) {
	// Accumulate sample for this channel
	float fMixerSample = 0.0f;

	for (auto& s : listActiveSamples) {
		// Calculate sample position
		s.nSamplePosition += (long) ((float) vecAudioSamples[s.nAudioSampleID - 1].wavHeader.nSamplesPerSec * fTimeStep);

		// If sample position is valid add to the mix
		if (s.nSamplePosition < vecAudioSamples[s.nAudioSampleID - 1].nSamples)
			fMixerSample += vecAudioSamples[s.nAudioSampleID - 1].fSample[(s.nSamplePosition * vecAudioSamples[s.nAudioSampleID - 1].nChannels) + nChannel];
		else
			s.bFinished = true; // Else sound has completed
	}

	// If sounds have completed then remove them
	listActiveSamples.remove_if([] (const sCurrentlyPlayingSample& s) {return s.bFinished; });

	// The users application might be generating sound, so grab that if it exists
	fMixerSample += onUserSoundSample(nChannel, fGlobalTime, fTimeStep);

	// Return the sample via an optional user override to filter the sound
	return onUserSoundFilter(nChannel, fGlobalTime, fMixerSample);
}

int ConsoleGameEngine::GetMouseX() {
	return m_mousePosX;
}
int ConsoleGameEngine::GetMouseY() {
	return m_mousePosY;
}
ConsoleGameEngine::sKeyState ConsoleGameEngine::GetMouse(int nMouseButtonID) {
	return m_mouse[nMouseButtonID];
}
bool ConsoleGameEngine::IsFocused() {
	return m_bConsoleInFocus;
}

int ConsoleGameEngine::Error(const wchar_t* msg) {
	wchar_t buf[256];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
	SetConsoleActiveScreenBuffer(m_hOriginalConsole);
	wprintf(L"ERROR: %s\n\t%s\n", msg, buf);
	return 0;
}

BOOL ConsoleGameEngine::CloseHandler(DWORD evt) {
	// Note this gets called in a seperate OS thread, so it must
	// only exit when the game has finished cleaning up, or else
	// the process will be killed before OnUserDestroy() has finished
	if (evt == CTRL_CLOSE_EVENT) {
		m_bAtomActive = false;

		// Wait for thread to be exited
		std::unique_lock<std::mutex> ul(m_muxGame);
		m_cvGameFinished.wait(ul);
	}
	return true;
}

Sprite::Sprite() {
	nWidth = 0;
	nHeight = 0;
	m_Glyphs = nullptr;
	m_Colours = nullptr;
}

Sprite::Sprite(int w, int h) {
	Create(w, h);
}

Sprite::Sprite(std::wstring sFile) {
	nWidth = 0;
	nHeight = 0;
	m_Glyphs = nullptr;
	m_Colours = nullptr;
	if (!Load(sFile))
		Create(8, 8);
}

void Sprite::Create(int w, int h) {
	nWidth = w;
	nHeight = h;
	m_Glyphs = new short[w * h];
	m_Colours = new short[w * h];
	for (int i = 0; i < w * h; i++) {
		m_Glyphs[i] = L' ';
		m_Colours[i] = FG_BLACK;
	}
}

void Sprite::SetGlyph(int x, int y, short c) {
	if (x < 0 || x >= nWidth || y < 0 || y >= nHeight)
		return;
	else
		m_Glyphs[y * nWidth + x] = c;
}

void Sprite::SetColour(int x, int y, short c) {
	if (x < 0 || x >= nWidth || y < 0 || y >= nHeight)
		return;
	else
		m_Colours[y * nWidth + x] = c;
}

short Sprite::GetGlyph(int x, int y) {
	if (x < 0 || x >= nWidth || y < 0 || y >= nHeight)
		return L' ';
	else
		return m_Glyphs[y * nWidth + x];
}

short Sprite::GetColour(int x, int y) {
	if (x < 0 || x >= nWidth || y < 0 || y >= nHeight)
		return FG_BLACK;
	else
		return m_Colours[y * nWidth + x];
}

short Sprite::SampleGlyph(float x, float y) {
	int sx = (int) (x * (float) nWidth);
	int sy = (int) (y * (float) nHeight - 1.0f);
	if (sx < 0 || sx >= nWidth || sy < 0 || sy >= nHeight)
		return L' ';
	else
		return m_Glyphs[sy * nWidth + sx];
}

short Sprite::SampleColour(float x, float y) {
	int sx = (int) (x * (float) nWidth);
	int sy = (int) (y * (float) nHeight - 1.0f);
	if (sx < 0 || sx >= nWidth || sy < 0 || sy >= nHeight)
		return FG_BLACK;
	else
		return m_Colours[sy * nWidth + sx];
}

bool Sprite::Save(std::wstring sFile) {
	FILE* f = nullptr;
	_wfopen_s(&f, sFile.c_str(), L"wb");
	if (f == nullptr)
		return false;

	fwrite(&nWidth, sizeof(int), 1, f);
	fwrite(&nHeight, sizeof(int), 1, f);
	fwrite(m_Colours, sizeof(short), nWidth * nHeight, f);
	fwrite(m_Glyphs, sizeof(short), nWidth * nHeight, f);

	fclose(f);

	return true;
}

bool Sprite::Load(std::wstring sFile) {
	delete[] m_Glyphs;
	delete[] m_Colours;
	nWidth = 0;
	nHeight = 0;

	FILE* f = nullptr;
	_wfopen_s(&f, sFile.c_str(), L"rb");
	if (f == nullptr)
		return false;

	std::fread(&nWidth, sizeof(int), 1, f);
	std::fread(&nHeight, sizeof(int), 1, f);

	Create(nWidth, nHeight);

	std::fread(m_Colours, sizeof(short), nWidth * nHeight, f);
	std::fread(m_Glyphs, sizeof(short), nWidth * nHeight, f);

	std::fclose(f);
	return true;
}

// Define our static variables
std::atomic<bool> ConsoleGameEngine::m_bAtomActive(false);
std::condition_variable ConsoleGameEngine::m_cvGameFinished;
std::mutex ConsoleGameEngine::m_muxGame;
