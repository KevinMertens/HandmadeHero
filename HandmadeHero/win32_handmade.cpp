#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#define local_persist static
#define global_variable static
#define internal static

#define Pi32 3.14159265359f

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch; 
	int BytesPerPixel;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XinputSetState_ = XInputSetStateStub;
#define XInputSetState XinputSetState_

#define DIRECT_SOUND_CREATE(DirectSoundCreate) HRESULT WINAPI DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXinput(void) {
	HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
	if (!XInputLibrary) {
		XInputLibrary = LoadLibrary("xinput1_3.dll");
	}
	if (XInputLibrary) {
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
	else {
		//TODO
	}
}

internal void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize) {
	HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
	if (DSoundLibrary) {
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
						OutputDebugString("Primary buffer format was set.\n");
					}
					else {
						//TODO
					}
				}
				else {
					//TODO
				}
			}
			else {
				//TODO
			}
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0))) {
				OutputDebugString("Secondary buffer created.\n");
			}
			else {
				//TODO
			}
		}
		else {
			//TODO
		}
	}
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
	win32_window_dimension Dimension;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Dimension.Width = ClientRect.right - ClientRect.left;
	Dimension.Height = ClientRect.bottom - ClientRect.top;
	return(Dimension);
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset) {
	uint8_t *Row = (uint8_t *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y) {
		uint32_t *Pixel = (uint32_t *)Row;
		for (int X = 0; X < Buffer->Width; ++X) {
			uint8_t Blue = X + XOffset;
			uint8_t Green = Y + YOffset;
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height) {
	if (Buffer->Memory) {
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	// Nagative Height -> top-down DIB, Positive Height -> bottom-up DIB
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	int BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight) {
	StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;
	switch (Message)
	{
		case WM_SIZE:
		{
			break;
		}
		case WM_DESTROY:
		{
			GlobalRunning = false;
			break;
		}
		case WM_CLOSE:
		{
			GlobalRunning = false;
			break;
		}
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
			EndPaint(Window, &Paint);
			break;
		}
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32_t VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) == 0);
			if (IsDown != WasDown) {
				if (VKCode == 'Z') {
				}
				else if (VKCode == 'Q') {
				}
				else if (VKCode == 'S') {
				}
				else if (VKCode == 'D') {
				}
				else if (VKCode == 'A') {
				}
				else if (VKCode == 'E') {
				}
				else if (VKCode == VK_UP) {
				}
				else if (VKCode == VK_DOWN) {
				}
				else if (VKCode == VK_LEFT) {
				}
				else if (VKCode == VK_RIGHT) {
				}
				else if (VKCode == VK_SPACE) {
					OutputDebugStringA("VK_SPACE: ");
					if (IsDown) {
						OutputDebugStringA("IsDown");
					}
					if (WasDown) {
						OutputDebugStringA("WasDown");
					}
					OutputDebugStringA("\n");
				}
				else if (VKCode == VK_ESCAPE) {
				}
			}
			bool AltKeyWasDown = ((LParam & (1 << 29)) != 0);
			if (VKCode == VK_F4 && AltKeyWasDown) {
				GlobalRunning = false;
			}
			break;
		}
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
			break;
		}
	}
	return(Result);
}

struct win32_sound_output {
	int SamplesPerSecond;
	int ToneHz;
	int16_t ToneVolume;
	uint32_t RunningSampleIndex;
	int WavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
};

internal void Win32FillsoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite) {
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16_t *SampleOut = (int16_t *)Region1;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
			float t = 2.0f * Pi32 * (float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float  SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			SoundOutput->RunningSampleIndex++;
		}
		SampleOut = (int16_t *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
			float t = 2.0f * Pi32 * (float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float  SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			SoundOutput->RunningSampleIndex++;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommanddLine, int ShowCode) {
	Win32LoadXinput();
	WNDCLASS WindowClass = {};
	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";
	if (RegisterClass(&WindowClass)) {
		HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
		if (Window) {
			HDC DeviceContext = GetDC(Window);

			int XOffset = 0;
			int YOffset = 0;

			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.ToneHz = 256;
			SoundOutput.ToneVolume = 3000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
			SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillsoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;
			while (GlobalRunning) {
				MSG Message;
				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) {
						GlobalRunning = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
						XINPUT_GAMEPAD *Gamepad = &ControllerState.Gamepad;
						bool DPadUp = (Gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
						bool DPadDown = (Gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
						bool DPadLeft = (Gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
						bool DPadRight = (Gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
						bool ButtonStart = (Gamepad->wButtons & XINPUT_GAMEPAD_START) != 0;
						bool ButtonBack = (Gamepad->wButtons & XINPUT_GAMEPAD_BACK) != 0;
						bool LeftShoulder = (Gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
						bool RightShoulder = (Gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
						bool ButtonA = (Gamepad->wButtons & XINPUT_GAMEPAD_A) != 0;
						bool ButtonB = (Gamepad->wButtons & XINPUT_GAMEPAD_B) != 0;
						bool ButtonX = (Gamepad->wButtons & XINPUT_GAMEPAD_X) != 0;
						bool ButtonY = (Gamepad->wButtons & XINPUT_GAMEPAD_Y) != 0;
						int16_t StickX = Gamepad->sThumbLX;
						int16_t StickY = Gamepad->sThumbLY;

						if (DPadUp) {
							++XOffset;
						}
						if (DPadDown) {
							--XOffset;
						}
						if (DPadRight) {
							++YOffset;
						}
						if (DPadLeft) {
							--YOffset;
						}
					}
				}

				/*XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);*/

				RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

				DWORD PlayCursor;
				DWORD WriteCursor;
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
					DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
					DWORD BytesToWrite;
					if (ByteToLock == PlayCursor) {
						BytesToWrite = 0;
					}
					else if(ByteToLock > PlayCursor) {
						BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
						BytesToWrite += PlayCursor;
					}
					else {
						BytesToWrite = PlayCursor - ByteToLock;
					}
					Win32FillsoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
				}
				
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
			}
		}
		else {
			//TODO
		}
	}
	else {
		//TODO
	}
	return(0);
}