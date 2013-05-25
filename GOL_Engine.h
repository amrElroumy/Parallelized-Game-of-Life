#include <iostream>
#include <fstream>
#include <string>
#include "MPI.h"
#include "Constants.h"
#include <assert.h>
using namespace std;

# define OFFSET(HEIGHT, WIDTH) WIDTH + HEIGHT * (_nCols + 2)

class GOL_Engine
{
private:
	bool** _grid;
	bool** _PaddedGrid;
	int _nCols;
	int _nRows;
	int _nPaddedCols;
	int _nPaddedRows;

	int _rank;
	int _size;

	static const int adjX[];
	static const int adjY[];

	bool isInMap (int x, int y)
	{
		return x >= 0 && x < _nCols && y >= 0 && y <_nRows;
	}

	string readMapConfig (string path, int& width, int& height)
	{
		std::ifstream fStream (path);
		fStream >> width >> height;

		std::string contents ( (std::istreambuf_iterator<char> (fStream) ), std::istreambuf_iterator<char>() );

		return contents;
	}

	void InitSlaveMap()
	{
		assert (_rank != MASTER);
		_grid = new bool*[_nRows];
		_PaddedGrid = new bool*[_nRows];

		for (int i=0; i<_nRows; i++)
		{
			_grid[i] = new bool[_nCols];
			_PaddedGrid[i] = new bool[_nCols];
		}

		for (int i=0; i<_nRows; i++)
		{
			for (int j=0; j<_nCols; j++)
			{
				_grid[i][j] = _PaddedGrid[i][j] = false;
			}
		}
	}

	void InitMasterMap (string mapPath)
	{
		assert (_rank == MASTER);

		string representation = readMapConfig (mapPath, _nCols, _nRows);

		_nPaddedRows = _nRows + 2;
		_nPaddedCols = _nCols + 2;

		clog << "[MASTER]: Read map configuration" << endl;

		_grid = new bool*[_nRows];
		_PaddedGrid = new bool*[_nPaddedRows];


		for (int i=0; i<_nRows; i++)
		{
			_grid[i] = new bool[_nCols];
		}

		for (int i=0; i<_nPaddedRows; i++)
		{
			_PaddedGrid[i] = new bool[_nPaddedCols];
		}

		int readIndex = 0;


		for (int y=0; y<_nRows && readIndex<representation.length(); y++)
		{
			for (int x=0; x<_nCols && readIndex<representation.length(); readIndex++)
			{
				if (representation[readIndex]!='\n')
				{ _grid[y][x++] = (bool) (representation[readIndex] - '0'); }
			}
		}
	}


public:
	GOL_Engine (int argc, char** argv, string mapPath)
	{
		MPI::Init (argc, argv);

		_rank = MPI::COMM_WORLD.Get_rank();
		_size = MPI::COMM_WORLD.Get_size();

		if (_rank == MASTER)
		{
			//if (_size == 1)
			//{
			//	cerr << "[MASTER]: Minimum number of machines is 2\nExitting.." << endl;
			//	MPI::Finalize();
			//	exit (EXIT_FAILURE);
			//}

			InitMasterMap (mapPath);

			clog << "[MASTER]: Finished initializing map" << endl;

			int nChunks = _nRows / _size;
			int dimensions[2] = {nChunks + 2, _nCols};

			MPI::Request* requests = new MPI::Request[_size-1];

			for (int i=1; i<_size - 1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Isend (dimensions, 2, MPI::INT, i, TAG_INITIALIZATION);
			}

			if (_size > 2)
			{
				int remainder = _nRows % _size;
				dimensions[0] = nChunks + remainder;
				requests[_size - 2] = MPI::COMM_WORLD.Isend (dimensions, 2, MPI::INT, _size - 1, TAG_INITIALIZATION);
			}

			MPI::Request::Waitall (_size-1, requests);
			delete[] requests;

			clog << "[MASTER]: Finished sending map dimensions to slaves" << endl;

			clog << "[MASTER]: Finished initializing process" << endl;
		}
		else
		{
			clog << "[Slave " << _rank << " ]: Initializing process " << endl;

			int dimensions[2];
			MPI::COMM_WORLD.Recv (dimensions, 2, MPI::INT, 0, TAG_INITIALIZATION);
			_nRows = dimensions[0];
			_nCols = dimensions[1];

			InitSlaveMap();

			clog << "[Slave " << _rank << " ]: Finished initializing process " << "Rows: " << _nRows << " Cols: " << _nCols << endl;
		}
	}

	/*GOL_Engine (const char* representation, int width, int height) : _width (width), _height (height)
	{
		_matrix = new bool[_width * _height];
		_doubleBuffer = new bool[_width * _height];

		for (int readIndex=0, writeIndex = 0; readIndex<strlen (representation); readIndex++)
		{
			if (representation[readIndex]!='\n')
			{ _matrix[writeIndex++] = (bool) (representation[readIndex] - '0'); }
		}
	}*/

	~GOL_Engine()
	{
		for (int i=0; i<_nRows+2; i++)
		{
			if (_grid != NULL)
			{ delete[] _grid[i]; }

			if (_PaddedGrid != NULL)
			{ delete[] _PaddedGrid[i]; }
		}

		MPI::Finalize();
	}

	void Display()
	{
		// Called only in MASTER so we neglect the ghost rows and columns
		for (int y=1; y<_nRows+1; y++)
		{
			for (int x=1; x<_nCols+1; x++)
			{
				cout << (int) _grid[y][x];
			}

			cout << endl;
		}

		cout << endl;
	}

	void InitGhostCells()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Initializing ghost rows" << endl;

			// copy left and right columns
			for (int i=1; i<_nRows+1; i++)
			{
				_grid[i][0] = _grid[i][_nCols];
				_grid[i][0] = _grid[i][_nCols];
			}

			// copy corners
			_grid[0][0]                =_grid[0][_nCols];
			_grid[0][_nRows+1]         =_grid[0][1];
			_grid[ _nRows+1][0]        =_grid[_nRows+1][_nCols];
			_grid[ _nRows+1][_nCols+1] =_grid[_nRows+1][1];

			// copy top and bottom
			for (int i=1; i<_nCols+1; i++)
			{
				_grid[0][i] = _grid[_nRows][i];
				_grid[0][i] = _grid[_nRows][i];
			}

			clog << "[MASTER]: Finished initializing ghost rows" << endl;
		}
	}

	void ForkDataToSlaves()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Forking data to slaves" << endl;

			int nChunks = _nRows / _size;
			int remainder = _nRows % _size;

			int start = 0; // The start index takes in account the index of the first ghost row

			MPI::Request* requests = new MPI::Request[_size-1];

			for (int i=1; i<_size-1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Isend (_grid[start], (nChunks + 2) * _nCols, MPI::INT, i, TAG_DATA);

				// Update the start index to stand on the row above the last sent row
				// to send it to the next process and be able to process the last sent row
				start += nChunks;
			}

			// The last process takes the normal chunk size + remaining rows
			requests[_size - 2] = MPI::COMM_WORLD.Isend (_grid[start], (nChunks + 2 + remainder) * _nCols, MPI::INT, _size - 1, TAG_DATA);

			MPI::Request::Waitall (_size-1, requests);
			//delete[] requests;

			clog << "[MASTER]: Finished forking data to slaves" << endl;

		}
		else
		{
			clog << "[Slave " << _rank << " ]: Receiving data from master" << endl;

			MPI::COMM_WORLD.Recv (_grid[0], ( (_nRows + 2) * _nCols), MPI::INT, 0, TAG_DATA);

			clog << "[Slave " << _rank << " ]: Received data from master" << endl;

		}
	}

	void ApplyRules()
	{
		if (_rank != MASTER)
			/*{
				clog << "[MASTER]: Applying rules" << endl;
			}
			else
			{*/
		{
			clog << "[Slave " << _rank << " ]: Applying rules" << endl;

			try
			{
				for (int y=0; y<_nRows; y++)
				{
					for (int x = 0; x<_nCols; x++)
					{
						int nAliveAdj = 0;

						for (int i=0; i<8; i++)
						{
							if (isInMap (adjX[i] + x, adjY[i] + y) && (_grid[ (adjY[i] + y)][ (adjX[i] + x)]) )
							{
								nAliveAdj++;
							}
						}

						if (nAliveAdj < 2 || nAliveAdj > 3)
						{ _PaddedGrid[y][x] = false; }
						else
							if ( (nAliveAdj == 2 && _grid[y][x] == true) || nAliveAdj == 3)
							{ _PaddedGrid[y][x] = true; }
							else
							{ _PaddedGrid[y][x] = false; }

					}
				}
			}
			catch
				(std::exception* e)
			{
				cerr << "[Slave " << _rank << " ]: " <<  e->what() << endl;
				MPI::Finalize();
			}

			swap (_PaddedGrid, _grid);

			/*if (_rank == MASTER)
			{
				clog << "[MASTER]: Finished applying rules" << endl;
			}
			else
			{*/
			clog << "[Slave " << _rank << " ]: Applying rules" << endl;
		}
	}

	void CombineDataToMaster()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Gathering data from slaves" << endl;

			int nChunks = _nRows / _size;
			int remainder = _nRows % _size;

			int start = 0; // The start index takes in account the index of the first ghost row

			MPI::Request* requests = new MPI::Request[_size-1];

			for (int i=1; i<_size-1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Irecv (_grid[start], (nChunks + 2) * _nCols, MPI::INT, i, TAG_DATA);

				// Update the start index to stand on the row above the last sent row
				// to send it to the next process and be able to process the last sent row
				start += nChunks;
			}

			// The last process takes the normal chunk size + remaining rows
			requests[_size - 2] = MPI::COMM_WORLD.Irecv (_grid[start], (nChunks + 2 + remainder) * _nCols, MPI::INT, _size - 1, TAG_DATA);

			MPI::Request::Waitall (_size-1, requests);

			try
			{
				delete[] requests;
			}
			catch
				(std::exception* e)
			{
				cerr << e->what() << endl;
				//MPI::Finalize();
			}

			clog << "[MASTER]: Finished combining data from slaves" << endl;

		}
		else
		{
			clog << "[Slave " << _rank << " ]: Sending data to master" << endl;

			MPI::COMM_WORLD.Send (_grid[0], (_nRows + 2) * _nCols, MPI::INT, 0, TAG_DATA);

			clog << "[Slave " << _rank << " ]: Sent data to master" << endl;
		}
	}

	void Dispose()
	{
		this->~GOL_Engine();
	}
};

const int GOL_Engine::adjX[] = { 1, -1, 0, 0, 1, 1, -1, -1};
const int GOL_Engine::adjY[] = { 0, 0, 1, -1, 1, -1, 1, -1};
