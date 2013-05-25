#include <iostream>
#include <fstream>
#include <string>
#include "GOL_Engine.h"
using namespace std;


int main (int argc, char** argv)
{
	string mapPath = DEFAULT_PATH;
	int nGenerations = N_GENERATIONS;

	GOL_Engine golEngine (argc, argv, mapPath);

	golEngine.InitGhostCells();

	if (golEngine._rank == MASTER)
	{
		golEngine.Display();
		golEngine.DisplayPadded();
	}

	//for (int i=0; i<nGenerations; i++)
	//{
	//	golEngine.InitGhostCells();
	//	golEngine.DisplayPadded();
	//	golEngine.ForkDataToSlaves();
	//	golEngine.ApplyRules();
	//	golEngine.CombineDataToMaster();
	//	golEngine.DisplayPadded();
	//}

	golEngine.Dispose();
	return 0;
}