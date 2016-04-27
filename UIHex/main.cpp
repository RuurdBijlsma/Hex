
#include <Windows.h>
#include <string>
#include <vector>
#include <stdlib.h>
#include <chrono>
#include <time.h>
#include <iostream>
#include <condition_variable>
#include <deque>
#include <future>


using namespace std;

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
//voor nieuwe stijl

const int boardSizeX = 11,
boardSizeY = 11;
float hexSize = 15,
gridSize = 30,
hexWidth = hexSize*sqrt(3),
hexHeight = 1.5 * hexSize,
gap = hexSize*.2;

vector<vector<int>> allMoves;



class ThreadPool;

class Worker {
	ThreadPool &pool;
public:
	Worker(ThreadPool &t) : pool(t) {};
	void operator()();
};

class ThreadPool {
	friend class Worker;

	vector<thread> workers;
	deque<function<int()>> tasks;

	condition_variable cv;

	int tasksLeft;
	bool stop;
public:
	ThreadPool();
	condition_variable allDone;
	mutex queue_mutex;
	void Enqueue(function<int()>);
	~ThreadPool();
};

vector<int> evals;

void Worker::operator()() {
	function<int()> task;

	while (true)
	{
		unique_lock<mutex> locker(pool.queue_mutex);

		pool.cv.wait(locker, [&]() { return !pool.tasks.empty() || pool.stop; });
		if (pool.stop) return;

		task = pool.tasks.front();
		pool.tasks.pop_front();
		locker.unlock();

		int val = task();

		locker.lock();
		evals.push_back(val);

		pool.tasksLeft--;

		if (pool.tasksLeft == 0)
			pool.allDone.notify_all();

	}
}

ThreadPool::ThreadPool()
{
	stop = false;
	tasksLeft = 0;
	for (size_t i = 0; i < thread::hardware_concurrency(); ++i)
		workers.push_back(thread(Worker(*this)));
}

ThreadPool::~ThreadPool()
{
	stop = true; // stop all threads
	cv.notify_all();

	for (auto &thread : workers)
		thread.join();
}

void ThreadPool::Enqueue(function<int()> task)
{
	unique_lock<mutex> lock(queue_mutex);
	tasks.push_back(task);
	++tasksLeft;
	lock.unlock();

	cv.notify_one();
}
ThreadPool pool;


class Hexagon {
	int X, Y,
		XPx, YPx;
	char Color;
	vector<Hexagon*> DirectNeighbours;
	vector<Hexagon*> VirtualNeighbours;

public:
	Hexagon(int x = 0, int y = 0, char type = 'g');
	void SetPx(int x, int y);
	void SetColor(char c);
	vector<vector<int>> AllNeighbours();
	int GetXPx();
	int GetYPx();
	vector<int> GetPos();
	char GetColor();
	int GetDistance(int x2, int y2);
	void AddDirect(Hexagon& child);
	void AddVirtual(Hexagon& child);
	vector<Hexagon*> GetDirect();
	vector<Hexagon*> GetVirtual();
};

void Hexagon::AddDirect(Hexagon& child) {
	DirectNeighbours.push_back(&child);
}
void Hexagon::AddVirtual(Hexagon& child) {
	VirtualNeighbours.push_back(&child);
}
vector<vector<int>> Hexagon::AllNeighbours() {
	vector<vector<int>> positions;
	for (int i = 0; i < DirectNeighbours.size(); i++)
	{
		positions.push_back(DirectNeighbours[i]->GetPos());
	}
	for (int i = 0; i < VirtualNeighbours.size(); i++)
	{
		positions.push_back(VirtualNeighbours[i]->GetPos());
	}
	return positions;
}
vector<Hexagon*> Hexagon::GetDirect() {
	return DirectNeighbours;
}
vector<Hexagon*> Hexagon::GetVirtual() {
	return VirtualNeighbours;
}

Hexagon::Hexagon(int x, int y, char type) {
	X = x;
	Y = y;
	Color = type;
}
vector<int> Hexagon::GetPos() {
	return{ X, Y };
}
int Hexagon::GetDistance(int x2, int y2) {
	int dx = X - x2;
	int dy = Y - y2;

	if (dx > 0 == dy > 0) {
		return abs(dx + dy);
	}
	else {
		return max(abs(dx), abs(dy));
	}
}
void Hexagon::SetPx(int x, int y) {
	XPx = x;
	YPx = y;
}
void Hexagon::SetColor(char c) {
	Color = c;
}
char Hexagon::GetColor() {
	return Color;
}
int Hexagon::GetXPx() {
	return XPx;
}
int Hexagon::GetYPx() {
	return YPx;
}

Hexagon board[boardSizeX][boardSizeY];
vector< vector <vector <Hexagon> > > copies;

class BridgeGrid {
	int BridgeWeight;
	vector<Hexagon*> Grid;
	vector<vector<int>> Visited = {};
	char Color;
	char Enemy;
	int ThreadIndex;
public:
	vector<vector<int>> MainBridge;//hier staan 2 coordinaten in, begin van brug en eind van brug
	BridgeGrid();
	vector<int> FindBridge();
	void CheckBridge(int x, int y);
	int GetBestEval(char color, int threadIndex);
	int EvalColor();
};

BridgeGrid::BridgeGrid() {
}

vector<int> BridgeGrid::FindBridge() {//vind het begin van een brug
	vector<int> bridgePos;
	for (int x = 0; x < 11; x++)
	{
		for (int y = 0; y < 11; y++)
		{
			if (copies[ThreadIndex][x][y].GetColor() == Color && Visited[x][y] == 0) {
				return{ x,  y };
			}
		}
	}
	return{ -1, -1 };
}
vector<vector<int>> DiffToHex(int xDif, int yDif) {//krijg blokkerende hexes voor virtuele connectie
	if (xDif == -2 && yDif == 1)
		return{ { -1, 1 }, { -1, 0 } };
	if (xDif == -1 && yDif == -1)
		return{ { -1, 0 },{ 0, -1 } };
	if (xDif == 1 && yDif == -2)
		return{ { 0, -1 },{ 1, -1 } };
	if (xDif == 2 && yDif == -1)
		return{ { 1, -1 },{ 1,0 } };
	if (xDif == 1 && yDif == 1)
		return{ { 1, 0 },{ 0, 1 } };
	if (xDif == -1 && yDif == 2)
		return{ { 0, 1 },{ -1, 1 } };
}
void BridgeGrid::CheckBridge(int x, int y) {//vind bruggen en geef evaulatie gebaseert op lengte van brug en bridgeWeight
	if (Visited[x][y] == 0) {
		Visited[x][y] = 1;
		Grid.push_back(&copies[ThreadIndex][x][y]);

		vector<Hexagon*> direct = copies[ThreadIndex][x][y].GetDirect();
		vector<Hexagon*> virtualN = copies[ThreadIndex][x][y].GetVirtual();
		for (int i = 0; i < direct.size(); i++)
		{
			char dColor = direct[i]->GetColor();
			vector<int> dPos = direct[i]->GetPos();
			if (dColor == Color && Visited[dPos[0]][dPos[1]] == 0) {
				CheckBridge(dPos[0], dPos[1]);
			}
		}
		for (int i = 0; i < virtualN.size(); i++)
		{
			char vColor = virtualN[i]->GetColor();
			vector<int> vPos = virtualN[i]->GetPos();
			//als er 1 tegenstander hex tussen zit weight toevoegen
			//als er 2 tegenstander hexes tussen zitten bridge niet maken
			vector<vector<int>> checks = DiffToHex(vPos[0] - x, vPos[1] - y);
			if (copies[ThreadIndex][x + checks[0][0]][y + checks[0][1]].GetColor() == Enemy && copies[ThreadIndex][x + checks[1][0]][y + checks[1][1]].GetColor() == Enemy) {
				//2 blocks, geen brug
			}
			else {

				if (vColor == Color && Visited[vPos[0]][vPos[1]] == 0) {
					if (copies[ThreadIndex][x + checks[0][0]][y + checks[0][1]].GetColor() == Enemy || copies[ThreadIndex][x + checks[1][0]][y + checks[1][1]].GetColor() == Enemy) {
						//1 block, mogelijke brug
						BridgeWeight += 30;//brug kan nog gered worden
					}
					BridgeWeight += 15;
					CheckBridge(vPos[0], vPos[1]);
				}
			}
		}
	}
}

class ABNode {
	vector<vector<int>> Moves;
	char Color;
	int A;//Alpha
	int B;//Beta
	int Eval;
	vector<int> Children;
	int Parent;
	int Id;
	int Depth;
	bool Terminal;
public:
	ABNode(vector<vector<int>> moves, char c, int depth);

	void AddMove(vector<int> move);
	void SetColor(char c);
	void SetChildren(vector<int> num);
	vector<int> GetChildren();
	void AddChild(int id);
	void SetId(int id);
	void SetParent(int id);
	int GetId();
	void SetEval(int eval);
	int GetEval(int threadIndex);
	int GetParent();
	void SetMoves(vector<vector<int>> moves);
	void SetDepth(int depth);
	void SetTerminal(bool isTerminal);
	bool GetTerminal();

	char GetColor();
	int GetDepth();
	vector<vector<int>> GetMoves();

	void SetA(int a);
	void SetB(int b);
};
ABNode::ABNode(vector<vector<int>> moves, char c, int depth) {
	Moves = moves;
	Color = c;
	Depth = depth;
}
vector<int> ABNode::GetChildren() {
	return Children;
}
void ABNode::AddChild(int id) {
	Children.push_back(id);
}
void ABNode::SetDepth(int depth) {
	Depth = depth;
}
void ABNode::AddMove(vector<int> move) {
	Moves.push_back(move);
}
void ABNode::SetColor(char c) {
	Color = c;
}
void ABNode::SetId(int id) {
	Id = id;
}
int ABNode::GetId() {
	return Id;
}
void ABNode::SetTerminal(bool isTerminal) {
	Terminal = isTerminal;
}
bool ABNode::GetTerminal() {
	return Terminal;
}
void ABNode::SetEval(int eval) {
	Eval = eval;
}
void ABNode::SetParent(int id) {
	Parent = id;
}
int ABNode::GetParent() {
	return Parent;
}

void ABNode::SetChildren(vector<int> num) {
	Children = num;
}
void ABNode::SetMoves(vector<vector<int>> moves) {
	Moves = moves;
}

char ABNode::GetColor() {
	return Color;
}
vector<vector<int>> ABNode::GetMoves() {
	return Moves;
}
int ABNode::GetDepth() {
	return Depth;
}

void ABNode::SetA(int a) {
	A = a;
}
void ABNode::SetB(int b) {
	B = b;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hwnd, button, button2, textField, undoButton, textInput;//global window handler vars
float pi = 3.1415926538;
char turn = 'r';

void DrawLine(int beginX, int beginY, int endX, int endY, HDC hdc, COLORREF color) {
	float deltaX = endX - beginX;
	float deltaY = endY - beginY;
	float rc;
	if (deltaX == 0) {
		rc = 0;
	}
	else {
		rc = deltaY / deltaX;
	}
	if ((abs(rc) > 1 || rc == 0) && deltaY != 0) {
		//lijn gaat meer dan 45 graden omhoog
		if (beginY > endY) {
			swap(beginX, endX);
			swap(beginY, endY);
		}

		if (rc != 0)
			rc = 1 / rc;


		for (int y = beginY; y <= beginY + abs(beginY - endY); y++)
		{
			SetPixel(hdc, beginX + rc*(y - beginY), y, color);
		}
	}
	else {
		//lijn gaat minder dan 45 graden omhoog
		if (beginX > endX) {
			swap(beginX, endX);
			swap(beginY, endY);
		}

		for (int x = beginX; x <= beginX + abs(beginX - endX); x++)
		{
			SetPixel(hdc, x, beginY + rc*(x - beginX), color);
		}
	}
}


void DrawPoly(int x, int y, int sides, float radius, HDC hdc, COLORREF color) {
	float rads = ((360 / sides * pi) / 180),
		beginX = x,
		beginY = y + radius;
	for (int i = 0; i < sides; i++)
	{
		float endX = x + sin(rads * (i + 1))*radius;
		float endY = y + cos(rads * (i + 1))*radius;

		DrawLine(beginX, beginY, endX, endY, hdc, color);

		beginX = endX;
		beginY = endY;
	}

}

void DrawCopyBoard(int threadIndex) {//nooit gecalled
	int posx = 50,
		posy = 140;
	HDC hdc = GetDC(hwnd);

	COLORREF color = RGB(220, 220, 220);
	float shiftx = 20;
	int xpx, ypx;
	for (int y = 0; y < boardSizeY; y++)
	{
		for (int x = 0; x < boardSizeX; x++)
		{
			xpx = posx + shiftx + x*(hexWidth + gap);
			ypx = posy + y*(hexHeight + gap);

			if (copies[threadIndex][x][y].GetColor() == 'b') {
				color = RGB(0, 0, 255);
			}
			else if (copies[threadIndex][x][y].GetColor() == 'r') {
				color = RGB(255, 0, 0);
			}
			else {
				color = RGB(220, 220, 220);
			}

			/*for (float i = 0; i <= hexSize; i += .099)
			{
			DrawPoly(xpx, ypx, 6, i, hdc, color);
			}*/
			DrawPoly(xpx, ypx, 6, hexSize, hdc, color);
		}
		shiftx += hexSize;
	}
}

int BridgeGrid::EvalColor() {
	//DrawCopyBoard(ThreadIndex);//debug
	int bestEval = 0;

	while (true) {
		//is er nog een bridge?
		vector<int> pos = FindBridge();
		if (pos == vector<int>{-1, -1}) {
			return bestEval;
		}
		//vind bridge
		Grid.clear();
		CheckBridge(pos[0], pos[1]);

		//meet huidige brug lengte ( in de goede richting )
		int bestDistance = 0;
		int dis;
		if (Color == 'r') {//het doel van rood is om van boven naar onder te gaan
			for (int i = 0; i < Grid.size(); i++)
			{
				for (int a = 0; a < Grid.size(); a++)
				{
					dis = Grid[i]->GetPos()[1] - Grid[a]->GetPos()[1] + 1;
					if (dis > bestDistance) {
						bestDistance = dis;
						MainBridge = { Grid[i]->GetPos(), Grid[a]->GetPos() };
					}
				}
			}
		}
		else {//blauw gaat van links naar rechts
			for (int i = 0; i < Grid.size(); i++)
			{
				for (int a = 0; a < Grid.size(); a++)
				{
					dis = Grid[i]->GetPos()[0] - Grid[a]->GetPos()[0] + 1;
					if (dis > bestDistance) {
						bestDistance = dis;
						MainBridge = { Grid[i]->GetPos(), Grid[a]->GetPos() };
					}
				}
			}
		}
		//stap 2: genereer eval van huidige brug
		int eval = bestDistance * bestDistance * 4 - BridgeWeight * (bestDistance / 7 + 1);//hij moet zo lang mogelijk zijn in zo weinig mogelijk zetten ( ik weet niet of de 20 perfect is )
		if (eval > bestEval) {
			bestEval = eval;
		}
	}
	return bestEval;
}
void DrawBoard(int posx, int posy, HDC hdc) {

	COLORREF color = RGB(220, 220, 220);
	float shiftx = 20;
	int xpx, ypx;
	for (int y = 0; y < boardSizeY; y++)
	{
		for (int x = 0; x < boardSizeX; x++)
		{
			xpx = posx + shiftx + x*(hexWidth + gap);
			ypx = posy + y*(hexHeight + gap);

			board[x][y].SetPx(xpx, ypx);
			/*for (float i = 0; i <= hexSize; i += .099)
			{
			DrawPoly(xpx, ypx, 6, i, hdc, color);
			}*/
			DrawPoly(xpx, ypx, 6, hexSize, hdc, color);
		}
		shiftx += hexSize;
	}
	DrawLine(posx, posy, posx + 150, posy + 255, hdc, RGB(0, 0, 255));
	DrawLine(posx + 330, posy, posx + 480, posy + 255, hdc, RGB(0, 0, 255));

	DrawLine(posx + 21, posy - 21, posx + 312, posy - 21, hdc, RGB(255, 0, 0));
	DrawLine(posx + 172, posy + 275, posx + 460, posy + 275, hdc, RGB(255, 0, 0));
}

void BeginDraw() {
	HDC hdc = GetDC(hwnd);
	//hexSize -> s
	//hexWidth -> d
	//hexHeight -> h
	DrawBoard(50, 140, hdc);

}
int IndexOf(vector<int> searchVector, int searchValue) {
	int pos = -1;
	for (int i = 0; i < searchVector.size(); i++)
	{
		if (searchVector[i] == searchValue)
			return i;
	}
	return pos;
}
vector<vector<int>> visited = { {} };
void ResetVisited(int threadIndex) {
	visited[threadIndex].assign(boardSizeX * boardSizeY, 0);
}
void ResetAllVisited() {
	for (int i = 0; i < visited.size(); i++)
	{
		visited[i].assign(boardSizeX * boardSizeY, 0);
	}
}

bool redFound = false;
bool redChecking = true;
void CheckRed(int x, int y, int target, int g, int d = 0) {

	if (redChecking && !redFound && g < boardSizeX*boardSizeY) {
		if (y == target) {
			redFound = true;
		}
		else {
			visited[0][y * boardSizeX + x] = 1;
			if (y < boardSizeY - 1) {
				// onder checken
				if (board[x][y + 1].GetColor() == 'r') {
					if (!visited[0][(y + 1) * boardSizeX + x] > d) {
						CheckRed(x, y + 1, target, ++g);
					}
				}
			}
			if (y < boardSizeY - 1 && x != 0) {
				// linksonder checken
				if (board[x - 1][y + 1].GetColor() == 'r') {
					if (!visited[0][(y + 1) * boardSizeX + (x - 1)] > d) {
						CheckRed(x - 1, y + 1, target, ++g);
					}
				}
			}
			if (x != 0) {
				// links checken
				if (board[x - 1][y].GetColor() == 'r') {
					if (!visited[0][y * boardSizeX + (x - 1)] > d) {
						CheckRed(x - 1, y, target, ++g);
					}
				}
			}
			if (x < boardSizeX - 1) {
				// rechts checken
				if (board[x + 1][y].GetColor() == 'r') {
					if (!visited[0][(y)* boardSizeX + (x + 1)] > d) {
						CheckRed(x + 1, y, target, ++g);
					}
				}
			}
			if (x < boardSizeX - 1 && y != 0) {
				// rechtsboven checken
				if (board[x + 1][y - 1].GetColor() == 'r') {
					if (!visited[0][(y - 1)* boardSizeX + (x + 1)] > d) {
						CheckRed(x + 1, y - 1, target, ++g);
					}
				}
			}
			if (y != 0) {
				// boven checken
				if (board[x][y - 1].GetColor() == 'r') {
					if (!visited[0][(y - 1)* boardSizeX + (x)] > d) {
						CheckRed(x + 1, y - 1, target, ++g);
					}
				}
			}
		}
	}
	else if (g >= boardSizeX*boardSizeY) {
		redChecking = false;
	}
}


bool blueFound = false;
bool blueChecking = true;
void CheckBlue(int x, int y, int target, int g, int threadIndex) {
	if (blueChecking && !blueFound && g < boardSizeX*boardSizeY) {
		if (x == target) {
			blueFound = true;
		}
		else {
			if (x < boardSizeX - 1) {
				// rechts checken
				if (copies[threadIndex][x + 1][y].GetColor() == 'b')
					CheckBlue(x + 1, y, target, ++g, threadIndex);
			}
			if (x < boardSizeX - 1 && y != 0) {
				// rechtsboven checken
				if (copies[threadIndex][x + 1][y - 1].GetColor() == 'b')
					CheckBlue(x + 1, y - 1, target, ++g, threadIndex);
			}
			if (y < boardSizeY - 1) {
				// onder checken
				if (copies[threadIndex][x][y + 1].GetColor() == 'b')
					CheckBlue(x, y + 1, target, ++g, threadIndex);
			}
			if (y != 0) {
				// boven checken
				if (copies[threadIndex][x][y - 1].GetColor() == 'b')
					CheckBlue(x, y - 1, target, ++g, threadIndex);
			}
			if (y < boardSizeY - 1 && x != 0) {
				// linksonder checken
				if (copies[threadIndex][x - 1][y + 1].GetColor() == 'b')
					CheckBlue(x - 1, y + 1, target, ++g, threadIndex);
			}
			if (x != 0) {
				// links checken
				if (copies[threadIndex][x - 1][y].GetColor() == 'b')
					CheckBlue(x - 1, y, target, ++g, threadIndex);
			}
		}
	}
	else if (g >= boardSizeX*boardSizeY) {
		blueChecking = false;
	}
}
//bool SearchRoute(int x, int y, char color) {
//	bool routeFound = false;
//	/*
//	vector<char> around=CheckAround(x, y);
//	for (int i = 0; i < around.size(); i++)
//	{
//		if(around[i]==color)
//	}*/
//	CheckBlue(x, y, )
//
//	return routeFound;
//}

char CheckWinner(char colorCheck, int threadIndex) {
	char winner = 'g';
	bool mogelijkPad = false;
	redFound = false;
	blueFound = false;
	ResetVisited(0);
	if (colorCheck == 'b') {
		for (int i = 0; i < boardSizeY; i++)//rechtse rij voor blauw checken
		{
			if (copies[threadIndex][boardSizeX - 1][i].GetColor() == 'b') {
				mogelijkPad = true;
				break;
			}
		}
		if (mogelijkPad) {
			for (int i = 0; i < boardSizeY; i++)//linkse rij voor blauw checken
			{
				if (copies[threadIndex][0][i].GetColor() == 'b') {
					//mogelijk pad
					//if (IndexOf(visited[0], i*boardSizeX) == -1) {//als de hex niet al onderzocht is ( trager dan niet checken )
						//int g = 0;
					blueChecking = true;
					CheckBlue(0, i, boardSizeX - 1, 0, threadIndex);
					if (blueFound) {
						return 'b';
					}
					//}
				}
			}
		}
	}
	else {
		mogelijkPad = false;
		for (int i = 0; i < boardSizeX; i++)//onderste rij voor rood checken
		{
			if (board[i][boardSizeY - 1].GetColor() == 'r') {
				mogelijkPad = true;
				//mogelijk begin
				break;
			}
		}
		if (mogelijkPad) {
			for (int i = 0; i < boardSizeX; i++)//bovenste rij voor rood checken
			{
				if (board[i][0].GetColor() == 'r') {
					//mogelijk pad
					//if (IndexOf(visited[0], i) == -1) {//als de hex niet al onderzocht is
						//int g = 0;
					redChecking = true;
					CheckRed(i, 0, boardSizeY - 1, 0);
					if (redFound) {
						return 'r';
					}
					//}
				}
			}
		}
	}
	return winner;
}







void CheckBlueBord(int x, int y, int target, int threadIndex, int g) {

	if (!blueFound&&g < 40) {
		if (x == target) {
			blueFound = true;
		}
		else if (y < 11 && x < 11 && y >= 0 && x >= 0) {
			visited[threadIndex][y * boardSizeX + x] = 1;
			if (x < boardSizeX - 1) {
				// rechts checken
				if (copies[threadIndex][x + 1][y].GetColor() == 'b') {
					if (!visited[threadIndex][(y)* boardSizeX + (x + 1)]) {
						CheckBlueBord(x + 1, y, target, threadIndex, ++g);
					}
				}
			}
			if (x < boardSizeX - 1 && y != 0) {
				// rechtsboven checken
				if (copies[threadIndex][x + 1][y - 1].GetColor() == 'b') {
					if (!visited[threadIndex][(y - 1) * boardSizeX + (x + 1)]) {
						CheckBlueBord(x + 1, y - 1, target, threadIndex, ++g);
					}
				}
			}
			if (y < boardSizeY - 1) {
				// onder checken
				if (copies[threadIndex][x][y + 1].GetColor() == 'b') {
					if (visited[threadIndex][(y + 1) * boardSizeX + x]) {
						CheckBlueBord(x, y + 1, target, threadIndex, ++g);
					}
				}
				int a = 0;
			}
			if (y != 0) {
				// boven checken
				if (copies[threadIndex][x][y - 1].GetColor() == 'b') {
					if (!visited[threadIndex][(y - 1) * boardSizeX + (x)]) {
						CheckBlueBord(x + 1, y - 1, target, threadIndex, ++g);
					}
				}
			}
			if (y < boardSizeY - 1 && x != 0) {
				// linksonder checken
				if (copies[threadIndex][x - 1][y + 1].GetColor() == 'b') {
					if (!visited[threadIndex][(y + 1) * boardSizeX + (x - 1)]) {
						CheckBlueBord(x - 1, y + 1, target, threadIndex, ++g);
					}
				}
			}
			if (x != 0) {
				// links checken
				if (copies[threadIndex][x - 1][y].GetColor() == 'b') {
					if (!visited[threadIndex][(y)* boardSizeX + (x - 1)]) {
						CheckBlueBord(x - 1, y, target, threadIndex, ++g);
					}
				}
			}
		}
	}
}
int BlueWinner(int threadIndex) {
	blueFound = false;
	ResetVisited(threadIndex);

	for (int y = 0; y < boardSizeY; y++)//linkse rij voor blauw checken
	{
		if (copies[threadIndex][0][y].GetColor() == 'b') {
			//mogelijk pad

			blueChecking = true;
			CheckBlueBord(0, y, boardSizeX - 1, threadIndex, 0);
			if (blueFound) {
				return 1;
			}
		}
	}

	return -1;
}


void CheckRedBord(int x, int y, int target, int threadIndex, int g) {

	if (!redFound&&g < 40) {
		if (y == target) {
			redFound = true;
		}
		else if (y < 11 && x < 11 && y >= 0 && x >= 0) {
			visited[threadIndex][y * boardSizeX + x] = 1;
			if (y < boardSizeY - 1) {
				// onder checken
				if (copies[threadIndex][x][y + 1].GetColor() == 'r') {
					if (visited[threadIndex][(y + 1) * boardSizeX + x]) {
						CheckRedBord(x, y + 1, target, threadIndex, ++g);
					}
				}
				int a = 0;
			}
			if (y < boardSizeY - 1 && x != 0) {
				// linksonder checken
				if (copies[threadIndex][x - 1][y + 1].GetColor() == 'r') {
					if (!visited[threadIndex][(y + 1) * boardSizeX + (x - 1)]) {
						CheckRedBord(x - 1, y + 1, target, threadIndex, ++g);
					}
				}
			}
			if (x != 0) {
				// links checken
				if (copies[threadIndex][x - 1][y].GetColor() == 'r') {
					if (!visited[threadIndex][(y)* boardSizeX + (x - 1)]) {
						CheckRedBord(x - 1, y, target, threadIndex, ++g);
					}
				}
			}
			if (x < boardSizeX - 1) {
				// rechts checken
				if (copies[threadIndex][x + 1][y].GetColor() == 'r') {
					if (!visited[threadIndex][(y)* boardSizeX + (x + 1)]) {
						CheckRedBord(x + 1, y, target, threadIndex, ++g);
					}
				}
			}
			if (x < boardSizeX - 1 && y != 0) {
				// rechtsboven checken
				if (copies[threadIndex][x + 1][y - 1].GetColor() == 'r') {
					if (!visited[threadIndex][(y - 1) * boardSizeX + (x + 1)]) {
						CheckRedBord(x + 1, y - 1, target, threadIndex, ++g);
					}
				}
			}
			if (y != 0) {
				// boven checken
				if (copies[threadIndex][x][y - 1].GetColor() == 'r') {
					if (!visited[threadIndex][(y - 1) * boardSizeX + (x)]) {
						CheckRedBord(x + 1, y - 1, target, threadIndex, ++g);
					}
				}
			}
		}
	}
}
int RedWinner(int threadIndex) {
	redFound = false;
	ResetVisited(threadIndex);

	for (int x = 0; x < boardSizeX; x++)//bovenste rij voor rood checken
	{
		if (copies[threadIndex][x][0].GetColor() == 'r') {
			//mogelijk pad

			redChecking = true;
			CheckRedBord(x, 0, boardSizeY - 1, threadIndex, 0);
			if (redFound) {
				return 1;
			}
		}
	}

	return -1;
}

int BridgeGrid::GetBestEval(char color, int threadIndex) {
	ThreadIndex = threadIndex;
	BridgeWeight = 0;
	MainBridge = {};

	int redWin = RedWinner(ThreadIndex);
	int blueWin = BlueWinner(ThreadIndex);

	if (color == 'r' && redWin == 1) {
		return 1000000;
	}
	else if (color == 'b'&&blueWin == 1) {
		return 1000000;
	}
	else if (color == 'r'&&blueWin == 1) {
		return -1000000;
	}
	else if (color == 'b'&&redWin == 1) {
		return -1000000;
	}



	Visited.assign(11, {});
	for (int i = 0; i < Visited.size(); i++)
	{
		Visited[i].assign(11, 0);
	}

	Color = color;
	if (color == 'b')
		Enemy = 'r';
	else
		Enemy = 'b';

	int posScore = EvalColor();

	char t = Enemy;
	Enemy = Color;
	Color = t;

	BridgeWeight = 0;

	int negScore = EvalColor();

	Color = Enemy;
	Enemy = t;

	return posScore - negScore;
}


bool CheckValid(int x, int y) {
	return x >= 0 && y >= 0 && x < 11 && y < 11;
}


vector<vector<int>> bestMoves;
vector<BridgeGrid> grids;
void startup() {

	copies.assign(121, {});
	visited.assign(121, {});


	for (int i = 0; i < 121; i++) {
		copies[i].assign(11, {});
		for (int j = 0; j < 11; j++)
		{
			copies[i][j].assign(11, NULL);
		}
	}

	for (int y = 0; y < boardSizeY; y++)
	{
		for (int x = 0; x < boardSizeX; x++)
		{
			board[x][y] = Hexagon(x, y, 'g');
			for (int i = 0; i < copies.size(); i++)
			{
				copies[i][x][y] = Hexagon(x, y, 'g');
			}
		}
	}

	//add neighbours to hexagons
	int nx,
		ny;
	for (int x = 0; x < 11; x++)
	{
		for (int y = 0; y < 11; y++)
		{
			//direct neighbours
			nx = x - 1;
			ny = y + 0;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);
			nx = x + 1;
			ny = y + 0;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);
			nx = x + 0;
			ny = y - 1;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);
			nx = x + 0;
			ny = y + 1;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);
			nx = x + 1;
			ny = y - 1;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);
			nx = x - 1;
			ny = y + 1;
			if (CheckValid(nx, ny))
				board[x][y].AddDirect(board[nx][ny]);

			//direct neighbours
			nx = x - 1;
			ny = y - 1;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);
			nx = x + 1;
			ny = y - 2;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);
			nx = x + 2;
			ny = y - 1;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);
			nx = x + 1;
			ny = y + 1;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);
			nx = x - 2;
			ny = y + 1;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);
			nx = x - 1;
			ny = y + 2;
			if (CheckValid(nx, ny))
				board[x][y].AddVirtual(board[nx][ny]);


			for (int i = 0; i < copies.size(); i++)
			{
				//direct neighbours
				nx = x - 1;
				ny = y + 0;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);
				nx = x + 1;
				ny = y + 0;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);
				nx = x + 0;
				ny = y - 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);
				nx = x + 0;
				ny = y + 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);
				nx = x + 1;
				ny = y - 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);
				nx = x - 1;
				ny = y + 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddDirect(copies[i][nx][ny]);

				//direct neighbours
				nx = x - 1;
				ny = y - 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
				nx = x + 1;
				ny = y - 2;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
				nx = x + 2;
				ny = y - 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
				nx = x + 1;
				ny = y + 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
				nx = x - 2;
				ny = y + 1;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
				nx = x - 1;
				ny = y + 2;
				if (CheckValid(nx, ny))
					copies[i][x][y].AddVirtual(copies[i][nx][ny]);
			}
		}
	}

	//MessageBox(hwnd, to_wstring(board[0][1].GetDistance(10, 5)).c_str(), L"Distance tussen 0, 0 en 10, 10", MB_OK);

	srand(time(NULL));
	BeginDraw();
}

// WinMain: The Application Entry Point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR nCmdLine, int nCmdShow)
{
	// Register the window class
	const wchar_t CLASS_NAME[] = L"WindowClass";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = CLASS_NAME;
	wc.hInstance = hInstance;
	RegisterClass(&wc);
	// Create the window
	hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Hex",//Titel
		WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX,
		// minimize		 kruisje etc  resize	   maximaliseren
		210, 140, 800, 600,//Positie en grootte op scherm (PosX, PosY, SizeX, SizeY)
		NULL, NULL, hInstance, NULL);

	if (hwnd == 0)
		return 0;
	// Show the window
	ShowWindow(hwnd, nCmdShow);
	nCmdShow = 1;
	// The Message loop
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

float GetDistance(int x1, int y1, int x2, int y2) {
	return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}
vector<int> ToGridpos(int posx, int posy) {
	vector<int> ret;
	ret = { 0,0 };
	float closest = 40000;

	for (int y = 0; y < boardSizeY; y++)
	{
		for (int x = 0; x < boardSizeX; x++)
		{
			float dis = GetDistance(posx, posy, board[x][y].GetXPx(), board[x][y].GetYPx());
			if (dis < closest) {
				closest = dis;
				ret[0] = x;
				ret[1] = y;
			}
		}
	}
	return ret;
	//return floor(pos / (hexSize*2))*hexSize * 2;
}
int lastMove = 60;

void Undo() {
	if (allMoves.size()>0) {
		HDC hdc = GetDC(hwnd);
		COLORREF grey = RGB(240, 240, 240);
		COLORREF grey2 = RGB(220, 220, 220);

		board[allMoves.back()[0]][allMoves.back()[1]].SetColor('g');
		for (float i = 0; i < hexSize; i += 0.05)
		{
			DrawPoly(board[allMoves.back()[0]][allMoves.back()[1]].GetXPx(), board[allMoves.back()[0]][allMoves.back()[1]].GetYPx(), 6, i, hdc, grey);
		}
		DrawPoly(board[allMoves.back()[0]][allMoves.back()[1]].GetXPx(), board[allMoves.back()[0]][allMoves.back()[1]].GetYPx(), 6, hexSize, hdc, grey2);
		allMoves.erase(allMoves.end() - 1);

		if (allMoves.size() != 0) {
			lastMove = allMoves.back()[0] + 11 * allMoves.back()[1];
		}
		if (turn == 'b')
			turn = 'r';
		else
			turn = 'b';
	}
}

void FillHex(int x, int y) {
	char color = board[x][y].GetColor();
	if (color == 'g') {
		allMoves.push_back({ x, y });
		HDC mouseHdc = GetDC(hwnd);

		int xpx = board[x][y].GetXPx();
		int ypx = board[x][y].GetYPx();
		lastMove = y * 11 + x;

		board[x][y].SetColor(turn);

		char winner = CheckWinner(turn, 0);

		if (winner == 'r') {
			MessageBox(hwnd, L"rood win", L"jo", MB_OK);
		}
		else if (winner == 'b') {
			MessageBox(hwnd, L"blauw win", L"jo", MB_OK);
		}
		COLORREF kleur;
		if (turn == 'r') {
			kleur = RGB(171, 17, 17);
			turn = 'b';
		}
		else {
			turn = 'r';
			kleur = RGB(13, 88, 226);
		}

		for (float i = 0; i <= hexSize*.9; i += .099)
		{
			DrawPoly(xpx, ypx, 6, i, mouseHdc, kleur);
		}
	}
}

void ResetBordCopy(int index) {
	for (int x = 0; x < boardSizeX; ++x)
	{
		for (int y = 0; y < boardSizeY; ++y)
		{
			copies[index][x][y].SetColor(board[x][y].GetColor());
		}
	}
	/*copies[index].assign(11, {});
	for (int x = 0; x < 11; x++)
	{
		copies[index][x].insert(copies[index][x].end(), &board[x][0], &board[x][11]);
	}*///keitraag
}

void ResetAllCopies(int n) {
	for (int x = 0; x < boardSizeX; ++x)
	{
		for (int y = 0; y < boardSizeY; ++y)
		{
			for (int i = 0; i < n; i++)
			{
				copies[i][x][y].SetColor(board[x][y].GetColor());
			}
		}
	}
}

int MontoRando(int n, int threadIndex, vector<vector<int>> moves, char lastTurn) {
	vector<int> teVullen;//indexes die nog gevuld moeten worden
	int eval = 0;
	char c = turn;

	for (int i = 0; i < n; i++)
	{
		ResetBordCopy(threadIndex);//problemo

		if (lastTurn == 'r') {
			for (int a = moves.size() - 1; a >= 0; a--)
			{
				if ((moves.size() - a) % 2 == 1) {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('r');
				}
				else {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('b');
				}
			}
		}
		else {
			for (int a = moves.size() - 1; a >= 0; a--)
			{
				if ((moves.size() - a) % 2 == 1) {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('b');
				}
				else {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('r');
				}
			}
		}

		for (int x = 0; x < 11; x++)
		{
			for (int y = 0; y < 11; y++)
			{
				if (board[x][y].GetColor() == 'g') {
					if (rand() % 2)
						copies[threadIndex][x][y].SetColor('b');
					else
						copies[threadIndex][x][y].SetColor('r');
				}
			}
		}
		eval += RedWinner(threadIndex);
	}

	return eval;
}

void BordCopyMove(int x, int y, char color, int threadIndex) {
	copies[threadIndex][x][y].SetColor(color);
}


int ABNode::GetEval(int threadIndex) {
	int eval = MontoRando(500, threadIndex, Moves, Color);
	return eval;
	return rand() % 500;
}


bool CheckValidMove(int x, int y, int threadIndex) {
	if (x > 10 || y > 10 || x < 0 || y < 0 || threadIndex < 0) {
		return false;
	}
	else {
		int a = 0;
		return x >= 0 && y >= 0 && x < 11 && y < 11 && copies[threadIndex][x][y].GetColor() == 'g';
	}
}
vector<vector<int>> PossibleMoves(int x, int y, int x2, int y2, int threadIndex) {
	vector<vector<int>> around = board[x][y].AllNeighbours();

	for (int i = around.size() - 1; i >= 0; i--)
	{
		if (copies[threadIndex][around[i][0]][around[i][1]].GetColor() != 'g') {
			around.erase(around.begin() + i);
		}
	}

	vector<vector<int>> toAdd = board[x2][y2].AllNeighbours();


	for (int a = toAdd.size() - 1; a >= 0; a--)
	{
		for (int i = around.size() - 1; i >= 0; i--)
		{
			if (toAdd[a] == around[i]) {
				toAdd.erase(toAdd.begin() + a);
				if (a > 0)
					a--;
			}
		}
	}

	//als er een goede brug is gevonden
	if (grids.size() > 0 && grids[threadIndex].MainBridge.size() > 0) {
		vector<vector<int>> addMore = grids[threadIndex].MainBridge;
		toAdd.insert(toAdd.end(), addMore.begin(), addMore.end());
		//voegt 2 uiteinden toe aan around
	}

	around.insert(around.end(), toAdd.begin(), toAdd.end());
	for (int i = around.size() - 1; i >= 0; i--)
	{
		if (copies[threadIndex][around[i][0]][around[i][1]].GetColor() != 'g') {
			around.erase(around.begin() + i);
		}
	}

	//check voor around.size()==0
	if (around.size() < 3) {
		for (int xx = 0; xx < 11; xx++)
		{
			if (copies[threadIndex][0][xx].GetColor() == 'g')
				around.push_back({ xx, 0 });

			if (copies[threadIndex][10][xx].GetColor() == 'g')
				around.push_back({ xx, 10 });
		}
		for (int yy = 0; yy < 11; yy++)
		{
			if (copies[threadIndex][0][yy].GetColor() == 'g')
				around.push_back({ 0, yy });

			if (copies[threadIndex][10][yy].GetColor() == 'g')
				around.push_back({ 10, yy });
		}
	}
	if (around.size() == 0) {
		for (int xx = 0; xx < 11; xx++)
		{
			for (int yy = 0; yy < 11; yy++)
			{
				if (copies[threadIndex][xx][yy].GetColor() == 'g')
					around.push_back({ xx, yy });
			}
		}
	}

	return around;
}

//int ComputerZet(char turn) {
//	int bestEval;
//	int rando;
//	vector<vector<int>> around;
//	int bestMove = -1;
//	COLORREF kleur = RGB(255, 0, 255);
//	HDC mouseHdc = GetDC(hwnd);
//
//	if (turn == 'b') {
//		bestEval = 100;
//
//		ResetBordCopy();
//		around = PossibleMoves(lastMove % 11, (int)(lastMove / 11));
//		for (int i = 0; i < around.size(); i++)
//		{
//			int x = around[i][0];
//			int y = around[i][1];
//
//			BordCopyMove(x, y, turn);
//			rando = MontoRando(100);
//			if (rando < bestEval) {
//				bestEval = rando;
//				bestMove = y * 11 + x;
//			}
//			DrawPoly(board[x][y].GetXPx(), board[x][y].GetYPx(), 6, hexSize, mouseHdc, kleur);
//			int jo = 1;
//		}
//	}
//	else {
//		bestEval = -100;
//
//		around = PossibleMoves(lastMove % 11, (int)(lastMove / 11));
//		for (int i = 0; i < around.size(); i++)
//		{
//			int x = around[i][0];
//			int y = around[i][1];
//			if (board[x][y].GetColor() == 'g') {
//				BordCopyMove(x, y, turn);
//				rando = MontoRando(100);
//				if (rando > bestEval) {
//					bestEval = rando;
//					bestMove = y * 11 + x;
//				}
//			}
//		}
//	}
//	return bestMove;
//}
vector<vector<ABNode>> nodes;
int CreateTree(int depth, int target, vector<vector<int>> moves, vector<int> addMoves, char color, int parentId, int threadIndex) {

	if (color == 'r')
		color = 'b';
	else
		color = 'r';

	moves.push_back(addMoves);
	ABNode parent = ABNode(moves, color, depth);

	int id = nodes[threadIndex].size();
	parent.SetId(id);

	nodes[threadIndex].push_back(parent);
	nodes[threadIndex].back().SetParent(parentId);

	if (depth < target) {
		nodes[threadIndex].back().SetTerminal(false);
		vector<vector<int>> around;
		if (allMoves.size()>1) {
			around = PossibleMoves(moves.back()[0], moves.back()[1], allMoves[allMoves.size() - 2][0], allMoves[allMoves.size() - 2][0], threadIndex);
		}
		else {
			around = PossibleMoves(moves.back()[0], moves.back()[1], 5, 5, threadIndex);
		}
		for (int i = 0; i < around.size(); i++)
		{
			BordCopyMove(around[i][0], around[i][1], color, threadIndex);
			CreateTree(depth + 1, target, moves, around[i], color, id, threadIndex);
			BordCopyMove(around[i][0], around[i][1], 'g', threadIndex);
		}
	}
	else {
		nodes[threadIndex].back().SetTerminal(true);
	}
	return 1;
}

int laag(int v1, int v2) {
	if (v1 > v2)
		return v2;
	else
		return v1;
}
int hoog(int v1, int v2) {
	if (v1 > v2)
		return v1;
	else
		return v2;
}

int AlphaBeta(ABNode node, int depth, int alpha, int beta, bool maximizer, int threadIndex) {
	if (depth == 0 || node.GetTerminal()) {
		int eval;

		/*if (turn == 'r') {*/
		vector<vector<int>> moves = node.GetMoves();

		ResetBordCopy(threadIndex);
		if (node.GetColor() == 'b') {
			for (int a = moves.size() - 1; a >= 0; a--)
			{
				if ((moves.size() - a) % 2 == 1) {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('b');
				}
				else {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('r');
				}
			}
			eval = grids[threadIndex].GetBestEval('b', threadIndex);
		}
		else if (node.GetColor() == 'r') {
			for (int a = moves.size() - 1; a >= 0; a--)
			{
				if ((moves.size() - a) % 2 == 1) {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('r');
				}
				else {
					copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('b');
				}
			}
			eval = grids[threadIndex].GetBestEval('r', threadIndex);
		}

		for (int a = 0; a < moves.size(); a++)
		{
			copies[threadIndex][moves[a][0]][moves[a][1]].SetColor('g');
		}
		/*}
		else {
			if (node.GetColor() == 'r') {
				eval = -node.GetEval(threadIndex);
			}
			else {
				eval = node.GetEval(threadIndex);
			}
		}*/


		return eval;
	}
	if (maximizer) {
		int bestValue = -99999;
		vector<int> children = node.GetChildren();
		for (int i = 0; i < children.size(); i++)
		{
			bestValue = hoog(bestValue, AlphaBeta(nodes[threadIndex][children[i]], depth - 1, alpha, beta, false, threadIndex));
			if (bestValue > alpha) {
				alpha = bestValue;
				bestMoves[threadIndex] = nodes[threadIndex][children[i]].GetMoves()[1];
				bestMoves[threadIndex].push_back(bestValue);
			}
			if (beta <= alpha)
				break;
		}
		return bestValue;
	}
	else {
		int bestValue = 99999;
		vector<int> children = node.GetChildren();
		for (int i = 0; i < children.size(); i++)
		{
			bestValue = laag(bestValue, AlphaBeta(nodes[threadIndex][children[i]], depth - 1, alpha, beta, true, threadIndex));
			if (bestValue < beta) {
				beta = bestValue;
				bestMoves[threadIndex] = nodes[threadIndex][children[i]].GetMoves()[1];
				bestMoves[threadIndex].push_back(bestValue);
			}
			if (beta <= alpha)
				break;
		}
		return bestValue;
	}
}
wchar_t nth_letter(int n)
{
	const wchar_t letters[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	return letters[n];
}

int StringToWString(wstring &ws, const string &s)
{
	wstring wsTmp(s.begin(), s.end());

	ws = wsTmp;

	return 0;
}

wstring MoveToText(vector<int> move) {
	wchar_t abc[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	wstring output;
	if (move.size() > 0) {
		output += abc[move[0]];
		output += to_wstring(move[1] + 1);
	}
	else {
		return L"Error";
	}
	return output;
}

vector<int> TextToMove(wstring text) {
	int x = towupper(text[0]) - 64;
	int y;
	if (text.length() == 3) {
		if (text[2] == '0')
			y = 10;
		else
			y = 11;
	}
	else {
		wchar_t ychr = text[1];
		wcstol(&ychr, NULL, 0);
		y = ychr - 48;
	}
	return{ x - 1, y - 1 };
}
vector<future<int>> abEvals;
vector<future<int>> trees;

void ComputerZet(int depth) {

	//root tree
	vector<vector<int>> around;
	if (allMoves.size() > 1) {
		around = PossibleMoves(lastMove % 11, lastMove / 11, allMoves[allMoves.size() - 2][0], allMoves[allMoves.size() - 2][0], 0);
	}
	else {
		around = PossibleMoves(lastMove % 11, lastMove / 11, 5, 5, 0);
	}

	nodes.assign(around.size(), {});
	bestMoves.assign(around.size(), {});

	grids.assign(around.size(), {});

	vector<vector<int>> moves = { {lastMove % 11, lastMove / 11} };

	char color;
	if (turn == 'r')
		color = 'b';
	else
		color = 'r';
	//end root

	for (int i = 0; i < around.size(); i++)
	{
		grids[i] = BridgeGrid();

		BordCopyMove(around[i][0], around[i][1], color, i);

		pool.Enqueue([depth, moves, around, color, i] {
			CreateTree(1, depth, moves, around[i], color, -1, i);

			for (int a = 0; a < nodes[i].size(); a++)
			{
				if (nodes[i][a].GetParent() != -1) {
					nodes[i][nodes[i][a].GetParent()].AddChild(nodes[i][a].GetId());
				}
			}

			ResetAllCopies(around.size());

			return AlphaBeta(nodes[i][0], depth, -99999, 99999, false, i);
		});
	}

	int bestIndex = 0;
	int tempBest = -99999;
	vector<int> bestMove;

	unique_lock<mutex> locker(pool.queue_mutex);
	pool.allDone.wait(locker);

	for (int i = 0; i < bestMoves.size(); i++)
	{
		if (bestMoves[i].size() == 3) {
			if (bestMoves[i][2] > tempBest) {
				tempBest = bestMoves[i][2];
				bestMove = { bestMoves[i][0], bestMoves[i][1] };
			}
		}
	}
	wstring w = MoveToText(bestMove);

	SetWindowText(textField, w.c_str());
	FillHex(bestMove[0], bestMove[1]);

	//na zet
	ResetBordCopy(0);
	evals.clear();

}
void temp() {
	auto start = chrono::high_resolution_clock::now();

	ResetBordCopy(0);
	BridgeGrid grid = BridgeGrid();
	int eval = grid.GetBestEval('r', 0);

	auto end = chrono::high_resolution_clock::now();
	auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

	MessageBox(hwnd, to_wstring(elapsed).c_str(), to_wstring(eval).c_str(), MB_OK);
}
// Window Procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char c;
	int t;
	int winner;

	switch (uMsg)
	{

	case WM_LBUTTONDOWN:
		POINT p;
		if (GetCursorPos(&p))
		{
			ScreenToClient(hwnd, &p);
			vector<int> pos = ToGridpos(p.x, p.y);

			FillHex(pos[0], pos[1]);
			auto start = chrono::high_resolution_clock::now();

			ResetBordCopy(0);
			ComputerZet(5);

			auto end = chrono::high_resolution_clock::now();
			auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

			//MessageBox(hwnd, to_wstring(elapsed).c_str(), L"Elapsed", MB_OK);
		}
		break;

	case WM_CREATE:

		/*textField = CreateWindow(L"STATIC",
		L"Mandelbrotmachine",
		WS_VISIBLE | WS_CHILD,
		20, 20, 200, 20,
		hwnd, NULL, NULL, NULL);

		textInput = CreateWindow(L"EDIT",
		L"500",
		WS_VISIBLE | WS_CHILD | WS_BORDER,
		20, 80, 200, 20,
		hwnd, NULL, NULL, NULL
		);*/

		button = CreateWindow(L"BUTTON",
			L"Pie rule",
			WS_VISIBLE | WS_CHILD | WS_BORDER,
			20, 20, 200, 20,
			hwnd, (HMENU)1, NULL, NULL
			);
		button2 = CreateWindow(L"BUTTON",
			L"Computer zet",
			WS_VISIBLE | WS_CHILD | WS_BORDER,
			20, 40, 200, 20,
			hwnd, (HMENU)2, NULL, NULL
			);
		undoButton = CreateWindow(L"BUTTON",
			L"Undo",
			WS_VISIBLE | WS_CHILD | WS_BORDER,
			20, 60, 200, 20,
			hwnd, (HMENU)3, NULL, NULL
			);
		textField = CreateWindow(L"STATIC",
			L"Output",
			WS_VISIBLE | WS_CHILD,
			20, 80, 200, 20,
			hwnd, NULL, NULL, NULL);
		textInput = CreateWindow(L"EDIT",
			L"",
			WS_VISIBLE | WS_CHILD | WS_BORDER,
			20, 100, 150, 20,
			hwnd, NULL, NULL, NULL);
		CreateWindow(L"BUTTON",
			L"Zet",
			WS_VISIBLE | WS_CHILD | WS_BORDER,
			170, 100, 50, 20,
			hwnd, (HMENU)4, NULL, NULL
			);

		break;

	case WM_COMMAND://voert uit bij actie

		switch (LOWORD(wParam))
		{

		case 1:
			//int eval;

			//eval = MontoRando(1000, 0) / 10;

			/*if (RedWinner(0) == 1) {
				MessageBox(hwnd, L"Blauw", to_wstring(eval).c_str(), MB_OK);
			}
			else {
				MessageBox(hwnd, L"Rood", to_wstring(eval).c_str(), MB_OK);
			}*/
			t = lastMove;
			c = turn;

			Undo();
			lastMove = t;
			turn = c;
			FillHex(lastMove % 11, lastMove / 11);


			break;

		case 2:
			ComputerZet(5);

			break;
		case 3:
			Undo();
			break;
		case 4:
			wchar_t buff[10];
			GetWindowText(textInput, buff, 10);
			vector<int> move = TextToMove(buff);
			ResetBordCopy(0);
			if (CheckValidMove(move[0], move[1], 0)) {
				FillHex(move[0], move[1]);
				ComputerZet(5);
			}
			else {
				MessageBox(hwnd, L"Incorrecte input", L"Error", MB_OK);
			}
			break;
		}

		break;

	case WM_DESTROY:PostQuitMessage(0); return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 0));//achtergrondkleur

		EndPaint(hwnd, &ps);
		startup();

	}return 0;

	case WM_CLOSE:
	{
		//if (MessageBox(hwnd, L"Do you want to exit?", L"Exit", MB_OKCANCEL) == IDOK)//Voor are you sure popup
		DestroyWindow(hwnd);
	}return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam); // Default Message Handling
}