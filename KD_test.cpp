#ifndef UNICODE
#define UNICODE
#define WM_KEYDOWN 0x0100
#define WM_LBUTTONDOWN 0x0201
#endif
// The code uses the Win32++ library for the GUI
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <random>
using namespace std;

// Shared resources for threading
wstring sharedGuess;
mutex guessMutex;
condition_variable cv;
atomic<bool> guessAvailable{ false };
atomic<bool> isCalculating{ true };

// GUI constants
const int ROWS = 6;
const int COLUMNS = 5;
vector<wstring> grid(ROWS, L"");
int currentRow = 0;
int currentColumn = 0;
// Global letter match vector
vector<vector<bool>> matchStatus(ROWS, vector<bool>(COLUMNS, false));

// Calculate how often all 26 letters appear in each position in a 5 letter word in a file
vector<vector<int>> letterFrequencyTable(string fileName)
{
    vector<vector<int>> letterFrequency{ {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };

    ifstream wordFile(fileName);
    string line;

    // Iterate through file line by line
    while (getline(wordFile, line))
    {
        // Iterate through line character by character and fill out letterFrequency
        for (int i = 0; i < 5; i++)
        {
            char c = line[i];
            letterFrequency[i][c - 'A']++;
        }
    }
    wordFile.close();
    return letterFrequency;
}

// Create a copy of the valid Wordle words list for later storage of remaining valid words after a guess
void generateFilteredWordList()
{
    ifstream wordFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\valid-wordle-words.txt");
    ofstream filteredFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");
    string line;

    while (getline(wordFile, line))
    {
        filteredFile << line << endl;
    }
    wordFile.close();
    filteredFile.close();
}

// Using the output of letterFrequencyTable(), calculate a value for each word depending on how common are the letters
// in the word in their position. Used for finding the best hint
void generateHintValues(vector<vector<int>> letterFrequency)
{
    ifstream filteredFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");
    ofstream hintValueFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\hint-values.txt");
    string line;
    char c;
    int value;

    while (getline(filteredFile, line))
    {
        value = 0;
        for (int i = 0; i < 5; i++)
        {
            c = line[i];
            value = value + letterFrequency[i][c - 'A'];    // value is the sum of frequency of each letter in it's respective position
        }
        hintValueFile << value << endl;
    }
    filteredFile.close();
    hintValueFile.close();
}

// Find the index of the highest value in the hint value file to find the respective word in the filtered word list
int findMaxValueIndex()
{
    ifstream hintValueFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\hint-values.txt");

    string line;
    int currentLine = 0;
    int maxLine = -1;
    int maxValue = -1;

    while (getline(hintValueFile, line))
    {
        currentLine++;
        int value = stoi(line);
        if (value > maxValue)
        {
            maxValue = value;
            maxLine = currentLine;
        }
    }
    hintValueFile.close();
    return maxLine;
}

// Select a random word from the valid Wordle words list to use as the correct answer
string randomWord()
{
    // Random number generator
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 14855);
    int randomValue = distrib(gen);

    ifstream wordFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\valid-wordle-words.txt");
    string line;
    int currentLine = 1;

    // When line number equals random number, read the line and close the file to stop reading
    while (getline(wordFile, line))
    {
        if (currentLine == randomValue)
        {
            wordFile.close();
            break;
        }
        currentLine++;
    }
    return line;
}

// Compare input guess to answer, filter valid words, recalculate frequency table and hints
void checkGuess(string guess, string answer, HWND hwnd)
{
    ifstream filteredFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");
    ofstream checkedFile("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\temp.txt");
    string line;

    // Track which letters in the guess and answer have been matched
    vector<bool> guessedMatched(COLUMNS, false);
    vector<bool> answerMatched(COLUMNS, false);

    // Mark exact matches in global variable
    for (int i = 0; i < COLUMNS; ++i)
    {
        if (guess[i] == answer[i])
        {
            matchStatus[currentRow - 1][i] = true;
            guessedMatched[i] = true;
            answerMatched[i] = true;
        }
        else
        {
            matchStatus[currentRow - 1][i] = false;
        }
    }

    // Filter valid words
    while (getline(filteredFile, line))
    {
        // Remove guess
        if (line == guess)
        {
            continue;
        }

        // Iterate through each character in a line
        bool isValid = true;
        for (int i = 0; i < COLUMNS; ++i)
        {
            // Remove words that don't have a matching letter in a position
            // (if letter is green, only keep words that have the same letter in the same position)
            if (matchStatus[currentRow - 1][i] && (line[i] != guess[i]))
            {
                isValid = false;
                break;
            }
            // Remove words that have a unmatched letter in a position
            // (if letter is white, only keep words that don't have the same letter in the same position)
            if (!matchStatus[currentRow - 1][i] && (line[i] == guess[i]))
            {
                isValid = false;
                break;
            }
        }
        // Add word if it passed the checks
        if (isValid)
        {
            checkedFile << line << endl;
        }
    }
    filteredFile.close();
    checkedFile.close();

    // Delete old filtered list and rename temp list to be the new filtered list
    remove("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");
    rename("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\temp.txt", "C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");

    // Update the frequency table and hint values from new filtered word list
    vector<vector<int>> newFrequencyTable = letterFrequencyTable("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\filtered-wordle-words.txt");
    generateHintValues(newFrequencyTable);

    // Force a GUI redraw to show green letters
    InvalidateRect(hwnd, NULL, TRUE);
}

// GUI functions
// Message handler for window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // Painting the content inside the window
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Custom font
            HFONT hFontOld, hFontNew;
            hFontNew = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            hFontOld = (HFONT)SelectObject(hdc, hFontNew);

            // Draw background
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW));
            
            // Draw Wordle grid
            for (int row = 0; row < ROWS; ++row)
            {
                for (int col = 0; col < COLUMNS; ++col)
                {
                    int left = 70 + col * 55;
                    int top = 40 + row * 55;
                    int right = left + 50;
                    int bottom = top + 50;

                    RECT rect = { left, top, right, bottom };

                    // If letters in guess and answer match, color the square green
                    if (matchStatus[row][col])
                    {
                        HBRUSH greenBrush = CreateSolidBrush(RGB(0, 128, 0));
                        FillRect(hdc, &rect, greenBrush);
                        SetBkColor(hdc, RGB(0, 128, 0));
                        DeleteObject(greenBrush);
                    }
                    // If letters in guess and answer don't match, color the square white
                    else
                    {
                        SetBkColor(hdc, RGB(255, 255, 255));
                        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
                    }

                    // Draw textboxes for each letter
                    if (col < static_cast<int>(grid[row].length()))
                    {
                        wstring character(1, grid[row][col]);
                        SIZE textSize;
                        GetTextExtentPoint32(hdc, character.c_str(), 1, &textSize);

                        int textX = left + (50 - textSize.cx) / 2;
                        int textY = top + (50 - textSize.cy) / 2;

                        DrawText(hdc, character.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }
            }
            SelectObject(hdc, hFontOld);
            DeleteObject(hFontNew);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // Handle key presses
        case WM_KEYDOWN:
        {
            // If a letter is pressed, draw it in a textbox
            if (wParam >= 'A' && wParam <= 'Z')
            {
                if (currentColumn < COLUMNS)
                {
                    grid[currentRow] += static_cast<wchar_t>(wParam);
                    ++currentColumn;
                    InvalidateRect(hwnd, NULL, TRUE);   // Trigger redraw after entering letter
                }
            }

            // If Backspace is pressed, delete last letter
            else if (wParam == VK_BACK)
            {
                if (currentColumn > 0)
                {
                    grid[currentRow].pop_back();
                    --currentColumn;
                    InvalidateRect(hwnd, NULL, TRUE);   // Trigger redraw after deleting letter
                }
            }

            // If Enter is pressed, submit word as a guess
            else if (wParam == VK_RETURN)
            {
                // Check if 5 letters are input
                if (currentRow < ROWS - 1 && currentColumn == COLUMNS)
                {
                    lock_guard<mutex> lock(guessMutex);
                    sharedGuess = grid[currentRow];
                    guessAvailable = true;
                    cv.notify_one();

                    ++currentRow;
                    currentColumn = 0;

                    InvalidateRect(hwnd, NULL, TRUE);   // Trigger UI update immediately after submitting guess
                }
            }
            return 0;
        }

        // Handle window closing
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 'main' function that handles calculations and functions, as well as the GUI
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Create filtered word list, calculate frequency table and hint values
    generateFilteredWordList();
    vector<vector<int>> frequencyTable = letterFrequencyTable("C:\\Users\\densl\\Documents\\ViA Stuff\\C++\\projects\\KD_test\\valid-wordle-words.txt");
    generateHintValues(frequencyTable);

    // Register the window class
    const wchar_t CLASS_NAME[] = L"Vordulis";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Calculate screen size for a more consistent window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = screenWidth / 4;
    int windowHeight = screenHeight / 1.5;

    int xPos = (screenWidth - windowWidth) / 2;
    int yPos = (screenHeight - windowHeight) / 2;

    // Create the window with paramaters
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Vordulis",
        WS_OVERLAPPEDWINDOW,
        xPos, yPos, windowWidth, windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL);

    ShowWindow(hwnd, nCmdShow);

    // Select random word as answer
    // string answer = randomWord();
    string answer = "PLACE";

    // Start thread for background calculations in parallel with the window
    thread calculationThread([&]()
        {
            while (isCalculating)
            {
                unique_lock<mutex> lock(guessMutex);
                cv.wait(lock, [] { return guessAvailable || !isCalculating; });

                if (!isCalculating)
                {
                    break;
                }
                string guess(sharedGuess.begin(), sharedGuess.end());
                guessAvailable = false;
                checkGuess(guess, answer, hwnd);
            } 
        });

    // Run message loop with window
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop background calculation thread
    isCalculating = false;
    cv.notify_one();
    calculationThread.join();
    return 0;
}