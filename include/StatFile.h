#ifndef Fresco_StatFile_h
#define Fresco_StatFile_h
#include <cstdio>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>

using namespace std;

enum StatType { MATRIX, LIST, FIRESIZE };

class StatFile {
	public:
		StatFile(string t, int sx, int sy, StatType st);
		void write(int y);
		string getTitle();
		void addStat(int sx, int sy, int s);
		void addStat(int nYear, int nRep, double dData, int nCause, int low, int mod, int highLSS, int highHSS);
		int getStat(int sx, int sy);
		int sendFile();
		int receiveFile();
		StatType statType;
	private:
		string title;
		int **stats;
		vector<vector<int> > statVector;
		int rows;
		int columns;
};
#endif
