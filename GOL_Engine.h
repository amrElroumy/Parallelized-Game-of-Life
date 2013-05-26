#pragma once

#pragma region Include
#include <iostream>
#include <fstream>
#include <string>
#include "MPI.h"
#include "Constants.h"
#include <assert.h>
#include <iosfwd>
#include <sstream>
#include <omp.h>
using namespace std;
#pragma endregion

#define OFFSET(Y, X) ((Y) * _nCols + (X))
#define OFFSETP(Y, X) ((Y) * _nPaddedCols + (X))
//#define OFFSETP(Y, X) ((Y+1) * _nPaddedCols + (X+1))

class GOL_Engine
{
private:
	bool* _grid;
	bool* _paddedGrid;

	int _nCols, _nRows;
	int _nPaddedCols, _nPaddedRows;

	int _size;
	int _rank;

public:
	int Rank() const { return _rank; }

private:

	//int OFFSETP(int Y, int X) { return Y * _nPaddedCols + X; }
	//int OFFSET(int Y, int X) { return Y * _nCols + X; }

	static const int adjX[];
	static const int adjY[];
	ofstream ofs;

	bool isInPaddedMap (int x, int y)
	{
		return x >= 0 && x < _nPaddedCols && y >= 0 && y <_nPaddedRows;
	}

	string readMapConfig (string path, int& width, int& height)
	{
		std::ifstream fStream (path);
		fStream >> width >> height;

		std::string contents ( (std::istreambuf_iterator<char> (fStream) ), std::istreambuf_iterator<char>() );

		return contents;
	}

	#pragma region Map Initialization
	void InitSlaveMap()
	{
		assert (_rank != MASTER);

		_grid = new bool[_nRows*_nCols];
		_paddedGrid = new bool[_nPaddedRows * _nPaddedCols];

		//for (i = 0; i<_nRows; i++)
		//{
		//	_grid[i] = new bool[_nCols];
		//	_paddedGrid[i] = new bool[_nPaddedCols];
		//}

		//for (; i<_nPaddedRows; i++)
		//{
		//	_paddedGrid[i] = new bool[_nPaddedCols];
		//}

		// Initialize maps
		for (int i=0; i<_nRows; i++)
		{
			for (int j=0; j<_nCols; j++)
			{
				//sherif edited
				/*_grid[OFFSET (i,j)] = _paddedGrid[OFFSET (i,j)] = -1;*/
				_grid[OFFSET (i,j)] = _paddedGrid[OFFSETP (i,j)] = false;
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

		_grid = new bool[_nRows * _nCols];
		_paddedGrid = new bool[_nPaddedRows * _nPaddedCols];

		/*	for (i = 0; i<_nRows; i++)
			{
				_grid[i] = new bool[_nCols];
				_paddedGrid[i] = new bool[_nPaddedCols];
			}

			for (; i<_nPaddedRows; i++)
			{
				_paddedGrid[i] = new bool[_nPaddedCols];
			}*/

		unsigned int readIndex = 0;

		for (int y=0; y<_nRows && readIndex<representation.length(); y++)
		{
			for (int x=0; x<_nCols && readIndex<representation.length(); readIndex++)
			{
				if (representation[readIndex]=='\n')
				{ continue; }

				_grid[OFFSET (y, x)] = (bool) (representation[readIndex] - '0');
				x++;
			}
		}
	}

	#pragma endregion

public:
	#pragma region Constructor
	GOL_Engine (int argc, char** argv, string mapPath)
	{
		MPI::Init (argc, argv);

		_rank = MPI::COMM_WORLD.Get_rank();
		_size = MPI::COMM_WORLD.Get_size();


		if (_rank == MASTER)
		{
			ofs.open ("[logfile] Master.txt");
			clog.rdbuf (ofs.rdbuf() );

			// Todo uncomment code
			//if (_size == 1)
			//{
			//	cerr << "[MASTER]: Minimum number of machines is 2\nExitting.." << endl;
			//	MPI::Finalize();
			//	exit (EXIT_FAILURE);
			//}

			InitMasterMap (mapPath);

			clog << "[MASTER]: Finished initializing map" << endl;


			// todo remove comment
			int nChunks = _nRows / (_size-1);
			clog << "nrows: " << _nRows << " nChunks: " <<   nChunks << "  nCols: " << _nPaddedCols << " size: " << _size - 1<< endl;

			int dimensions[2] = {nChunks, _nCols};

			MPI::Request* requests = new MPI::Request[_size-1];

			for (int i=1; i<_size - 1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Isend (dimensions, 2, MPI::INT, i, TAG_INITIALIZATION);
			}

			if (_size > 2)
			{
				int remainder = _nRows % (_size-1);
				dimensions[0] = nChunks + remainder;
				clog << "Sending dimensions to slave " << _size - 1 << " D[0]: " << dimensions[0] << " D[1]: " << dimensions[1] << "REM " << remainder << endl;
				requests[_size - 2] = MPI::COMM_WORLD.Isend (dimensions, 2, MPI::INT, _size - 1, TAG_INITIALIZATION);
			}

			MPI::Request::Waitall (_size-1, requests);

			try
			{
				delete[] requests;
			}
			catch
				(std::exception* e)
			{
				cerr << e->what() << endl;
				MPI::Finalize();
			}

			clog << "[MASTER]: Finished sending map dimensions to slaves" << endl;

			clog << "[MASTER]: Finished initializing process" << endl;
		}
		else
		{
			stringstream s;
			s << "[logfile] Slave " << _rank << ".txt";
			ofs.open (s.str() );
			clog.rdbuf (ofs.rdbuf() );
			cerr.rdbuf (ofs.rdbuf() );

			clog << "[Slave " << _rank << " ]: Initializing process " << endl;

			int dimensions[2];
			MPI::COMM_WORLD.Recv (dimensions, 2, MPI::INT, 0, TAG_INITIALIZATION);
			_nRows = dimensions[0];
			_nCols = dimensions[1];
			_nPaddedRows = _nRows + 2;
			_nPaddedCols = _nCols + 2;

			InitSlaveMap();

			clog << "[Slave " << _rank << " ]: Finished initializing process " << "Rows: " << _nRows << " Cols: " << _nCols << endl;
		}
	}
	#pragma endregion

	#pragma region Destructor
	~GOL_Engine()
	{
		if (_grid != NULL)
		{
			/*for (int i=0; i<_nRows; i++)
			{
				delete[] _grid[i];
			}*/

			delete[] _grid;
		}

		if (_paddedGrid != NULL)
		{
			/*for (int i=0; i<_nPaddedRows; i++)
			{
				delete[] _paddedGrid[i];
			}*/

			delete[] _paddedGrid;
		}

		ofs.close();

		MPI::Finalize();
	}
	#pragma endregion

	#pragma region Logging Functions
	void DisplayPaddedL()
	{
		clog << "Padded: \n";

		// Called only in MASTER so we neglect the ghost rows and columns
		for (int y=0; y<_nPaddedRows; y++)
		{
			for (int x=0; x<_nPaddedCols; x++)
			{
				clog << (int) _paddedGrid[OFFSETP (y, x)];
			}

			clog << endl;
		}

		clog << endl;
	}

	void DisplayPadded()
	{
		for (int y=0; y<_nPaddedRows; y++)
		{
			for (int x=0; x<_nPaddedCols; x++)
			{
				cout << (int) _paddedGrid[OFFSETP (y,x)];
			}

			cout << endl;
		}

		cout << endl;
	}

	void DisplayL()
	{
		// Called only in MASTER so we neglect the ghost rows and columns
		clog << "Grid: \n";

		for (int y=0; y<_nRows; y++)
		{
			for (int x=0; x<_nCols; x++)
			{
				clog << (int) _grid[OFFSET (y,x)];
			}

			clog << endl;
		}

		clog << endl;
	}

	void Display()
	{
		// Called only in MASTER so we neglect the ghost rows and columns
		for (int y=0; y<_nRows; y++)
		{
			for (int x=0; x<_nCols; x++)
			{
				cout << (int) _grid[OFFSET (y,x)];
			}

			cout << endl;
		}

		cout << endl;
	}

	#pragma endregion

	void InitGhostCells()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Initializing ghost rows" << endl;

			// copy left and right columns
			for (int i=1; i<_nRows + 1; i++)
			{
				_paddedGrid[OFFSETP (i, 0)] = _grid[OFFSET (i-1, _nCols-1)];			// Left ghost column from rightmost column
				_paddedGrid[OFFSETP (i,_nPaddedCols - 1)] = _grid[OFFSET (i-1,0)];	    // Right ghost column from leftmost column
			}

			// copy top and bottom
			for (int i=1; i<_nCols + 1; i++)
			{
				_paddedGrid[OFFSETP (0,i)] = _grid[OFFSET (_nRows-1,i-1)];
				_paddedGrid[OFFSETP (_nPaddedRows - 1,i)] = _grid[OFFSET (0,i-1)];
			}

			// Copy the actual cells to the padded grid
			for (int y=0; y<_nRows; y++)
			{
				for (int x=0; x<_nCols; x++)
				{
					_paddedGrid[OFFSETP (y+1,x+1)] = _grid[OFFSET (y,x)];
				}
			}

			// copy corner cells by mirroring diagonals
			_paddedGrid[OFFSETP (0,0)]								  =_grid[OFFSET (_nRows - 1, _nCols-1)];
			_paddedGrid[OFFSETP (0,_nPaddedCols - 1)]				  =_grid[OFFSET (_nRows - 1, 0)];
			_paddedGrid[OFFSETP (_nPaddedRows - 1, 0)]				  =_grid[OFFSET (0,_nCols-1)];
			_paddedGrid[OFFSETP (_nPaddedRows - 1, _nPaddedCols - 1)] =_grid[OFFSET (0,0)];

			clog << "[MASTER]: Finished initializing ghost rows" << endl;
		}
	}

	void ForkDataToSlaves()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Forking data to slaves" << endl;

			int start = 0; // The start index takes in account the index of the first ghost row
			int nChunks = (_nRows / (_size-1) ) + 2;

			clog << "nChunks: " << nChunks << "  nPaddedCols: " << _nPaddedCols;

			DisplayPaddedL();

			MPI::Request* requests = new MPI::Request[_size-1];

			for (int i=1; i<_size-1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Isend (&_paddedGrid[OFFSETP (start, 0)], (nChunks) * _nPaddedCols , MPI::INT, i, TAG_DATA_FORK);

				// Update the start index to stand on the row above the last sent row
				// to send it to the next process and be able to process the last sent row
				start += nChunks - 2;
			}

			if (_size > 2)
			{
				// The last process takes the normal chunk size + remaining rows
				int remainder = _nRows % (_size-1);
				requests[_size - 2] = MPI::COMM_WORLD.Isend (&_paddedGrid[OFFSETP (start, 0)], (nChunks + remainder) * _nPaddedCols, MPI::INT, _size - 1, TAG_DATA_FORK);

				clog << "remainder: " << remainder << endl;
			}

			// Wait for all threads to receive the data from the master thread
			MPI::Request::Waitall (_size-1, requests);
			delete[] requests;

			clog << "[MASTER]: Finished forking data to slaves" << endl;

		}
		else
		{
			clog << "[Slave " << _rank << " ]: Receiving data from master" << " nPaddedRows: " << _nPaddedRows << "nPaddedCols: " << _nPaddedCols << endl;

			MPI::COMM_WORLD.Recv (_paddedGrid, ( (_nPaddedRows) * _nPaddedCols), MPI::INT, 0, TAG_DATA_FORK);

			clog << "[Slave " << _rank << " ]: Received data from master" << endl;
			DisplayPaddedL();
		}
	}

	void ApplyRules()
	{
		if (_rank != MASTER)
		{
			clog << "[Slave " << _rank << " ]: Applying rules" << endl;

			try
			{
				//#pragma omp parallel
				{
					//int tid = omp_get_thread_num();
					//cout << "Thread no. " << tid << " of " <<  omp_get_num_threads() << endl;

					try
					{
						//#pragma omp for
						for (int y=1; y<_nPaddedRows - 1; y++)
						{
							//cout << "iteration no. " << y << endl;

							for (int x = 1; x<_nPaddedCols - 1; x++)
							{
								int nAliveAdj = 0;

								for (int i=0; i<8; i++)
								{
									//cout << "i: " << i << endl;
									//cout << "y: " << y << " x:" << x << endl;

									if (isInPaddedMap (adjX[i] + x, adjY[i] + y) && (_paddedGrid[ OFFSETP ( (adjY[i] + y), (adjX[i] + x) )]) )
									{
										nAliveAdj++;
									}
								}

								if (nAliveAdj < 2 || nAliveAdj > 3)
								{ _grid[OFFSET (y-1, x-1)] = false; }
								else
									if ( (nAliveAdj == 2 && _paddedGrid[OFFSETP (y,x)] == true) || nAliveAdj == 3)
									{ _grid[OFFSET (y-1,x-1)] = true; }
									else
									{ _grid[OFFSET (y-1,x-1)] = false; }
							}
						}
					}
					catch
						(std::exception* e)
					{
						cout << "Exception " << e->what() << endl;
					}
					catch
						(MPI::Exception* e)
					{
						cout << "Exception " << e->Get_error_string() << endl;
					}

					this->DisplayL();
				}
			}
			catch
				(std::exception* e)
			{
				cerr << "[Slave " << _rank << " ]: " <<  e->what() << endl;
				MPI::Finalize();
			}

			//swap (_paddedGrid, _grid);

			clog << "[Slave " << _rank << " ]: Finished Applying rules" << endl;
		}
	}

	void CombineDataToMaster()
	{
		if (_rank == MASTER)
		{
			clog << "[MASTER]: Gathering data from slaves" << endl;

			int nChunks = _nRows / (_size-1);
			int remainder = _nRows % (_size-1);

			int start = 0; // The start index takes in account the index of the first ghost row

			MPI::Request* requests = new MPI::Request[_size-1];

			clog << "[MASTER]: nChunks: " << nChunks << " nCols: " << _nCols << " remainder: " << remainder << endl;

			for (int i=1; i<_size-1; i++)
			{
				requests[i-1] = MPI::COMM_WORLD.Irecv (&_grid[OFFSET (start, 0)], (nChunks) * _nCols, MPI::INT, i, TAG_DATA_COMBINE);

				// Update the start index to stand on the row above the last sent row
				// to send it to the next process and be able to process the last sent row
				start += nChunks;
			}

			// The last process takes the normal chunk size + remaining rows
			requests[_size - 2] = MPI::COMM_WORLD.Irecv (&_grid[OFFSET (start, 0)], (nChunks + remainder) * _nCols, MPI::INT, _size - 1, TAG_DATA_COMBINE);

			MPI::Request::Waitall (_size-1, requests);

			try
			{
				delete[] requests;
			}
			catch
				(std::exception* e)
			{
				cerr << e->what() << endl;
				MPI::Finalize();
			}

			clog << "[MASTER]: Finished combining data from slaves" << endl;
		}
		else
		{
			clog << "[Slave " << _rank << " ]: Sending data to master" << endl;

			MPI::COMM_WORLD.Send (_grid, (_nRows) * _nCols, MPI::INT, 0, TAG_DATA_COMBINE);

			clog << "[Slave " << _rank << " ]: Sent data to master" << endl;
		}
	}

	void Dispose()
	{
		clog.flush();
		cerr.flush();
		this->~GOL_Engine();
		exit (EXIT_SUCCESS);
	}
};

const int GOL_Engine::adjX[] = { 1, -1, 0,  0, 1,  1, -1, -1};
const int GOL_Engine::adjY[] = { 0,  0, 1, -1, 1, -1,  1, -1};
