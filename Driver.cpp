#include <iostream>
#include <fstream>
#include <string>
using namespace std;

class GOL_Engine
{
private:
	bool* _matrix;
	bool* _doubleBuffer;
	int _width;
	int _height;

	static const int adjX[];
	static const int adjY[];

	bool isInMap (int x, int y)
	{
		return x >= 0 && x < _width && y >= 0 && y <_height;
	}

public:
	GOL_Engine (const char* representation, int width, int height) : _width (width), _height (height)
	{
		_matrix = new bool[_width * _height];
		_doubleBuffer = new bool[_width * _height];

		for (int readIndex=0, writeIndex = 0; readIndex<strlen (representation); readIndex++)
		{
			if (representation[readIndex]!='\n')
			{ _matrix[writeIndex++] = (bool) (representation[readIndex] - '0'); }
		}
	}

	~GOL_Engine()
	{
		if (_matrix != NULL)
		{ delete[] _matrix; }

		if (_doubleBuffer != NULL)
		{ delete[] _doubleBuffer; }
	}

	void Display()
	{
		for (int i=0; i<_height; i++)
		{
			for (int j=0; j<_width; j++)
			{
				cout << (int) _matrix[i * _width + j];
			}

			cout << endl;
		}

		cout << endl;
	}

	void Iterate()
	{
		for (int y=0; y<_height; y++)
		{
			for (int x = 0; x<_width; x++)
			{
				int nAliveAdj = 0;

				for (int i=0; i<8; i++)
				{
					if (isInMap (adjX[i] + x, adjY[i] + y) && (_matrix[ (adjY[i] + y) * _width + (adjX[i] + x)]) )
					{
						nAliveAdj++;
					}
				}

				if (nAliveAdj < 2 || nAliveAdj > 3)
				{ _doubleBuffer[y * _width + x] = false; }
				else
					if ( (nAliveAdj == 2 && _matrix[y*_width + x] == true) || nAliveAdj == 3)
					{ _doubleBuffer[y * _width + x] = true; }
					else
					{ _doubleBuffer[y * _width + x] = false; }

			}
		}

		swap (_doubleBuffer, _matrix);
	}
};

const int GOL_Engine::adjX[] = { 1, -1, 0, 0, 1, 1, -1, -1};
const int GOL_Engine::adjY[] = { 0, 0, 1, -1, 1, -1, 1, -1};

string readFile (char* path, int& width, int& height)
{
	std::ifstream fStream (path);
	fStream >> width >> height;
	std::string contents ( (std::istreambuf_iterator<char> (fStream) ), std::istreambuf_iterator<char>() );
	return contents;
}
int main()
{
	int width, height;
	string mapConfig = readFile ("config.txt", width, height);
	GOL_Engine gol (mapConfig.c_str(), width, height);
	gol.Display();
	gol.Iterate();
	gol.Display();
	gol.Iterate();
	gol.Display();
	gol.Iterate();
	gol.Display();
	gol.Iterate();
	gol.Display();
	gol.Iterate();
	gol.Display();
	return 0;
}