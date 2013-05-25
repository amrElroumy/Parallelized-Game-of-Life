#include <iostream>
#include <fstream>
#include <string>
#include "GOL_Engine.h"
using namespace std;


int main (int argc, char** argv)
{
	string mapPath = DEFAULT_PATH;
	int nGenerations = N_GENERATIONS;
	
	GOL_Engine golEngine(argc, argv, mapPath);

	for (int i=0; i<nGenerations; i++)
	{
		golEngine.InitGhostCells();
		golEngine.ForkDataToSlaves();
		golEngine.ApplyRules();
		golEngine.CombineDataToMaster();
		golEngine.DisplayPadded();
	}

	golEngine.Dispose();
	return 0;
}