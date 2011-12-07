// CInterface.cpp

// This module contains the code for the CInterface and associated classes.  The basic functionality of the 
// CInterface class is to provide a means for the user to run the code with all the program information in 
// an external file.  This allows the user to tweak the code without recompiling.  To implement this, an
// associated array way chosed that stored a variable data type.  A class was created (CVariable) that stored
// data of types long, long[], double, double[], string, and boolean.  Then an associated array class was 
// created (CAssoc) which stored that data using a string identifier to label the data.  Finally CInterface,
// provides a meta interface to the associative array which includes a parser to read in the configuration
// file, and functions for retreiving data of different types from the array.  

// The original intention of CInterface was to handle all (most) of the I/O routines that would be needed by
// the model.  To facilitate this, a few other functions were added during development, including a function
// which streamlined the opening and initializing of files, and checking files for consistancy.  I am sure 
// more functions will get added as the I/O needs of the model go up.

#include "Except.h"
#include "Interface.h"
#include "Poco/Path.h"


// Constants
const char ResSep[] = " ={},\";\t";						// Reserved characters
const char FloatSep[] = ".eEdD";						// Characters delineating floating point numbers from ints

// ============== CInterface =========================
void CInterface::Initialize (const std::string &basePath, const std::string &fileName) {
	_fifName = fileName;
	ParseFile(GetFullPath(basePath, fileName).c_str());
}

void CInterface::ParseFile (const char *pszFileName) {
	
	// This function parses and interprets the contents of the .fif file specified in the first argument. The
	// parsing is done by a worker function on a line by line basis.  The results of the parsed line are then 
	// run through an series of if statements to determine what the line says.  This filter is rather rudimentary,
	// and some assumptions are made about the line structure, however, it is reasonably intelligent about
	// reporting errors.  Once a line is parsed and interpreted, it is added to the associative array.  This
	// function also handles it's own errors because the simple error handler cannot handle the more complex
	// error messages generated by these routines.
	
	std::fstream fpFIF;						// Input file stream
	char buf[BUFSIZE];						// Temporary storage for the line
	char pcWordList[BUFSIZE][BUFSIZE];		// Temporary storege for the list of words in the line
	int nLineNumber = 0;					// The line number
	CVariable *Value = NULL;				// The data to add to the associative array

	// Temporary variables for data extracted from line
	char *pcTmp;							// String
	char **ppcTmp;							// Array of strings
	double *pdTmp;							// Array of doubles
	bool *pbTmp;							// Array of boolean
	int *pnTmp;								// Array of integers

	// Need to open the file manually, because the associated array isn't created and therefore we can't use CInterface::OpenFile
    fpFIF.open(pszFileName, std::ios::in);
	if (!fpFIF.is_open()) throw Exception(Exception::FILEBAD,std::string("Unable to open FIF file " + ToS(pszFileName)).c_str());

	while (!fpFIF.eof()) {
		fpFIF.getline(buf, BUFSIZE);
		nLineNumber++;
		if (int nLen = (int)strlen(buf)) {
			if (nLen == BUFSIZE-1)			// Check that we caught the EOL otherwise give an error
				throw Exception(Exception::INITFAULT, "Exceeded line length of " + ToS(BUFSIZE) + " characters on line " + ToS(nLineNumber) + " in " + pszFileName);

			int nWords = ParseLine(buf, pcWordList, nLen);
			
			if (nWords > 0) {		// Check that first word is a "semi" valid identifier - i.e. it doesn't contain any reserved symbols
				if ( strpbrk(pcWordList[0],ResSep) ) {
					throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Invalid IDENTIFIER found: " + pcWordList[0]);
					continue;
				}
			} else continue;
			
			if (nWords > 1) {		// Check the second word is equals sign
				if (pcWordList[1][0] != '=') {
					throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '=' but found: " + pcWordList[1]);
					continue;
				}
			} else {
				throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '=' but found: end of line\n");
				continue;
			}

			if (nWords == 2) {		// Unexpected end of line error
				throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected VALUE but found: end of line\n");
				continue;
			}

			if (nWords == 3) {		// We have a simple assignment i.e. not strings or arrays
				Tag tType = CheckType(pcWordList[2]);
				switch (tType) {
				case nTag :
					Value = new CVariable((int)strtol(pcWordList[2],NULL,0));
					break;
				case dTag :
					Value = new CVariable(strtod(pcWordList[2],NULL));
					break;
				case bTag :
					if (pcWordList[2][0] == 't' || pcWordList[2][0] == 'T')
						Value = new CVariable(true);
					else if (pcWordList[2][0] == 'f' || pcWordList[2][0] == 'F')
						Value = new CVariable(false);
					break;
				default:															// This includes string type, and UNKNOWN since we only have 3 words
					throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected VALUE but found: " + pcWordList[2] + "\n");
					continue;
				}
			}
				
			// We have a simple string, a simple array or an error
			if (nWords == 4 || nWords == 5) {												
				if (pcWordList[2][0] == '\"') {										// Potential string data
					if (pcWordList[3][0] =='\"')									// Empty String
					{
						size_t destSize = strlen("")+1; 
						pcTmp = new char[destSize];
						strcpy(pcTmp,"");

						//pcTmp = new char[1];
						//pcTmp[0] = 0;
					}
					else 
					{
						if ( strpbrk(pcWordList[3],ResSep) ) {						// Check the assignment does not contain any reserved seperators
							throw Exception(Exception::INITFAULT, "Error line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected STRING VALUE but found: " + pcWordList[3] + "\n");
							continue;
						}
						if ( pcWordList[4][0] != '\"') {							// Check for a closing string
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '\"' but found: " + pcWordList[4] + "\n");
							continue;
						}
						size_t destSize = strlen(pcWordList[3])+1; 
						pcTmp = new char[destSize];
						strcpy(pcTmp,pcWordList[3]);
					}
					Value = new CVariable(pcTmp);
				} else if (pcWordList[2][0] != '{') {								// Make sure we don't have a simple array
					if (pcWordList[nWords-1][0] == '\"') {							// Closed in a string so assume a string was intended
						throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '\"' but found: " + pcWordList[2] + "\n");
						continue;
					} else if (pcWordList[nWords-1][0] == '}') {					// Closed in a brace so assume an array was intended
						throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '{' but found: " + pcWordList[2] + "\n");
						continue;
					} else {														// Run out of good guesses
						throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected VALUE but found: " + pcWordList[2] + "\n");
						continue;
					}
				}
			}

			if (!Value) {																// The only option left is that we have an array of some type but it might be malformed
				// Compute the number of words between the braces and check for a closing brace bracket
				int nVars = 0;
				if (pcWordList[nWords-1][0] != '}') {
					throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '}' but found: end of line\n");
					continue;
				}
				while (pcWordList[nVars+3][0] != '}' && nVars < nWords-3) nVars++;
				if (nVars == nWords-3) {
					throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '}' but found: end of line\n");
					continue;
				}

				// Try and determine the type of the array
				bool bValid = true;														// Is this array well formed
				int nAdj = 3;															// Adjusts the alignment when errors are encountered
				bool bFloat = false;													// Type of numeric arrays (is it float)

				switch (CheckType(pcWordList[3])) {
				case nTag :
				case dTag :	// *** Numeric data ***
					nVars = (int)ceil(nVars/2.);
					for (int i = 0; ; i++) {
						int nType = CheckNumber(pcWordList[2*i+nAdj]);
						if (!nType) {
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected NUMERIC VALUE but found: " + pcWordList[2*i+nAdj] + "\n");
							bValid = false;
							if (pcWordList[2*i+(nAdj--)][0] == '}') break;
						} else if (nType == dTag) {
							bFloat = true;
						}
						if ( pcWordList[2*i+nAdj+1][0] != ',' ) {						// Check for a comma or close braces
							if (pcWordList[2*i+nAdj+1][0] == '}' && i>=nVars-1) break;	// This will be the usual exit from the loop
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected ',' but found: " + pcWordList[2*i+nAdj+1]  + "\n");
							bValid = false;
							if (pcWordList[2*i+(nAdj--)+1][0] == '}') break;
						}
					}
					if (!bValid) continue;												// If there is an error, discard this line and keep going

					if (bFloat) {														// Create a double array
						pdTmp = new double[nVars];
						for (int i = 0; i < nVars; i++)
							pdTmp[i] = strtod(pcWordList[2*i+3],NULL);
						Value = new CVariable (pdTmp, nVars);
					} else {															// End create double array, start integer array
						pnTmp = new int[nVars];
						for (int i = 0; i < nVars; i++)
							pnTmp[i] = strtol(pcWordList[2*i+3],NULL,0);
						Value = new CVariable (pnTmp, nVars);
					}																	// End integer array
					break;

				case bTag:	// *** Boolean data ***
					nVars = (int)ceil(nVars/2.);
					for (int i = 0; ; i++) {
						if (CheckType(pcWordList[2*i+nAdj]) != bTag) {
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected BOOLEAN VALUE but found: " + pcWordList[2*i+nAdj] + "\n");
							bValid = false;
							if (pcWordList[2*i+nAdj][0] == '}') break;
							if (pcWordList[2*i+nAdj][0] == ',') nAdj--;					// Only adjust if this is not a word rather than a corrupted word
						}

						if ( pcWordList[2*i+nAdj+1][0] != ',' ) {						// Check for a comma or close braces
							if (pcWordList[2*i+nAdj+1][0] == '}' && i>=nVars-1) break;	// This will be the usual exit from the loop
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected ',' but found: " + pcWordList[2*i+nAdj+1] + "\n");
							bValid = false;
							if (pcWordList[2*i+(nAdj--)+1][0] == '}') break;
						}
					}
					if (!bValid) continue;												// If there is an error, discard this line and keep going

					pbTmp = new bool[nVars];
					for (int i = 0; i < nVars; i++) {
						if (pcWordList[2*i+3][0] == 't' || pcWordList[2*i+3][0] == 'T')	// Assign a boolean state
							pbTmp[i]= true;
						else if (pcWordList[2*i+3][0] == 'f' || pcWordList[2*i+3][0] == 'F')
							pbTmp[i]= false;
					}
					Value = new CVariable (pbTmp, nVars);
					break;

				default:	// *** String data ***
				// Check for the empty array
					if (nVars == 0) {														// Check the case of an empty array
						if (pcWordList[3][0] != '}') {
							throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '}' but found: end of line\n");
							continue;
						}
					} else {
						nVars = (int)ceil(nVars/4.);
						for (int i = 0; ; i++) {
							if ( pcWordList[4*i+nAdj][0] != '\"' ) {						// Check for a opening quote
								throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '\"' but found: " + pcWordList[4*i+nAdj]  + "\n");
								bValid = false;
								if (pcWordList[4*i+(nAdj--)][0] == '}') break;				// We are guarenteed an close brace bracket by nVars counting algorithm
							}
							if ( strpbrk(pcWordList[4*i+nAdj+1],ResSep) ) {					// Check the assignment does not contain any reserved seperators
                                //TODO...
								if ( pcWordList[4*i+nAdj+1][0] != '\"' ) {					// Check the case of the empty string
                                    throw Exception(Exception::INITFAULT, "Error line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected STRING VALUE but found: " + pcWordList[4*i+nAdj+1] + "\n");
									bValid = false;
									if (pcWordList[4*i+nAdj+1][0] == '}') break;
								} else {
									//throw Exception(Exception::INITFAULT, "Error line " + ToS(nLineNumber) + " in " + pszFileName + ": Empty string not permitted\n");
									//bValid = false;
									if (pcWordList[4*i+(nAdj--)+1][0] == '}') break;
								}
							}
							if ( pcWordList[4*i+nAdj+2][0] != '\"' ) {						// Check for a closing quote
								throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected '\"' but found: " + pcWordList[4*i+nAdj+2]  + "\n");
								bValid = false;
								if (pcWordList[4*i+(nAdj--)+2][0] == '}') break;
							}
							if ( pcWordList[4*i+nAdj+3][0] != ',' ) {						// Check for a comma or close braces
								if (pcWordList[4*i+nAdj+3][0] == '}' && i>=nVars-1) break;	// This will be the usual exit from the loop
								throw Exception(Exception::INITFAULT, "Line " + ToS(nLineNumber) + " in " + pszFileName + ": Expected ',' but found: " + pcWordList[4*i+nAdj+3] + "\n");
								bValid = false;
								if (pcWordList[4*i+(nAdj--)+3][0] == '}') break;
							}
						}
					}
					if (!bValid) continue;												// If there is an error, discard this line and keep going

					ppcTmp = new char*[nVars];
					for (int i = 0; i < nVars; i++) {
                        size_t destSize = strlen(pcWordList[4*i+4]) + 1;
						ppcTmp[i] = new char[destSize];
//						strcpy_s (ppcTmp[i], destSize, pcWordList[4*i+4]);
						strcpy(ppcTmp[i], pcWordList[4*i+4]);
					}
					Value = new CVariable(ppcTmp,nVars);
					break;
				}
			}

			// Add the data to the associative array
            size_t destSize = strlen(pcWordList[0])+1;
			pcTmp = new char[destSize];
			//strcpy_s (pcTmp, destSize, pcWordList[0]);
			strcpy(pcTmp, pcWordList[0]);
            IOAssoc.Put(pcTmp, Value);
            Value = NULL;
		}	// End handle line
	}	// End while not end of file
	fpFIF.close();
}

int CInterface::ParseLine(char *pszLine, char pcWords[][BUFSIZE], int nLen) {

	// This function parses an individual line from the input file into words.  It performs a very similar function
	// to strtok, however, it was necessary to have seperators also returned as tokens.  This did not leave enough
	// space in the array to insert null terminating characters.  As a result, the whole process could not be done 
	// "inline".  This function gets tokens, and then call GetWord which copies new word into that memory.  This 
	// function expects an array of word buffers to be passed with memory already allocated to avoid memory leak
	// problems.  The basic format of a sentance is one of the following for simple, string and array types.  Also
	// whitespace can optionally be inserted at any point before or after tokens.

	// [IDENTIFIER] [=] [NUMERIC VALUE] {;Comment}
	// [IDENTIFIER] [=] ['"'] [STRING VALUE] ['"'] {;Comment}
	// [IDENTIFIER] [=] ['{'] [NUMERIC VALUE] {,[NUMERIC VALUE]} ['}'] {;Comment}
	
	// The function returns the number of words in the sentance.  If an error occurs, it returns the current number
	// of words processed.  This should be caught by the handler and reported as an error.

	int nPos = 0;
	int nWords = 0;
	char *pcEndWord;
	
	while (pszLine[nPos] == ' ' || pszLine[nPos] == '\t') nPos++;		// Eat white space
	while (pcEndWord = strpbrk(pszLine + nPos, ResSep)) {				// Find the next seperator
		if (pcEndWord - pszLine == nPos)
			nPos += GetWord (pcWords[nWords++], pszLine + nPos, 1);		// The word is a seperator so handle it specially
		else
			nPos += GetWord (pcWords[nWords++], pszLine + nPos, pcEndWord - pszLine - nPos);	// Hand the word

		if (pcWords[nWords-1][0] == ';') return nWords-1;				// If we found a comment, stop processing

		while (pszLine[nPos] == ' ' || pszLine[nPos] == '\t') nPos++;	// Move to the next position - eat whitespace
	}
	
	if (nPos != nLen)													// Get the last word if necessary
		GetWord (pcWords[nWords++], pszLine + nPos, nLen - nPos);

	return nWords;
}

int CInterface::GetWord (char pcDest[], const char *pcSource, const int nLen) {

	// This function basically performs a strncpy, but also appends a null terminating character.  It returns
	// the number of characters copied.
	//strncpy_s(pcDest, BUFSIZE, pcSource, nLen);
	strncpy(pcDest, pcSource, nLen);
	pcDest[nLen] = '\0';
	return nLen;
}

Tag CInterface::CheckType (const char *pcWord) {

	// This function attempts to determine the type of the word passed to it.  It returns the correct tag from
	// ResSepTag if possible, otherwise it returns ResSepUNKNOWN.

	// Check if it a well formed number
	Tag tType = CheckNumber (pcWord);
	if (tType != UNKNOWN) return tType;

	// Check if it is a boolean value
	if ( pcWord[0] == 't' || pcWord[0] == 'T')
		if (strlen(pcWord) == 1)
			return bTag;
		else
			if (!strcmp("true",pcWord) || !strcmp("True",pcWord) || !strcmp("TRUE",pcWord)) return bTag;
	if ( pcWord[0] == 'f' || pcWord[0] == 'F')
		if (strlen(pcWord) == 1)
			return bTag;
		else
			if (!strcmp("false",pcWord) || !strcmp("False",pcWord) || !strcmp("FALSE",pcWord)) return bTag;

	// Check that the word does not contain any reserved seperators
	if ( !strpbrk(pcWord,ResSep) ) return sTag;

	// Otherwise it is unknown
	return UNKNOWN;
}

Tag CInterface::CheckNumber(const char *pcWord) {

	// This function checks a word to see if it is a well formed number.  It returns ResSepUNKNOWN if the word is not
	// a well formed number, ResSepnTag if it is an integer type, and ResSepdTag  if it a floating point type.  
	// This number could be of hex, oct, or dec bases if it is an integer determined by the leading digits.  A float in
	// using either fixed or scientific notation.  The formats expected are for long and double respectively are :

	// [{whitespace}] [{+ | �}] [{0 [{ x | X }]}] [digits]
	// [{whitespace}] [{+ | �}] [digits] [{ .[{digits}] }] [{ [d | D | e | E] [{+ | �}] digits }]

	int nPos = 0;
	bool Hex = false, Oct = false;

	while (pcWord[nPos] == ' ' || pcWord[nPos] == '\t') nPos++;		// Eat white space
	if (pcWord[nPos] == '+' || pcWord[nPos] == '-') nPos++;			// Check sign
	if (!pcWord[nPos]) return UNKNOWN;								// There needs to be at least one digit
	if (pcWord[nPos] == '0')										// Hex or octal
		if (pcWord[nPos+1] == 'x' || pcWord[nPos+1] == 'X') {		// Hex format for integer data
			Hex = true;
			nPos += 2;
		} else if (pcWord[nPos+1] == 0) {							// The possibility of a single 0
			return nTag;
		} else if (pcWord[nPos+1] != '.') {							// Octal format
			Oct = true;
			nPos += 1;
		} 

	const char *pcPos = &(pcWord[nPos]);							// Eat mantessa digits (integer or float)
	char *pcDigits = NULL;
	if (Hex) {														// Hex case
		pcDigits = new char[23];
		//strcpy_s (pcDigits, 23, "0123456789abcdefABCDEF");
		strcpy(pcDigits, "0123456789abcdefABCDEF");
	} else if (Oct) {												// Octal case
		pcDigits = new char[9];
		//strcpy_s (pcDigits, 9, "01234567");
		strcpy(pcDigits, "01234567");
	} else {														// Decimal case
		pcDigits = new char[11];
		//strcpy_s (pcDigits, 11, "0123456789");
		strcpy(pcDigits, "0123456789");
	}
	while (!strcspn(pcPos,pcDigits)) {
		nPos++;
		if (!*(++pcPos)) {
			delete[] pcDigits;
			return nTag;											// Ran off the end of the string so therefore legit integer
		}
	}
	delete[] pcDigits;
	if (Hex || Oct) return UNKNOWN;									// If we weren't digits to the end of an integer then it isn't a number

	if (pcWord[nPos] == '.') {										// If we have a decimal digit, we need to process right of the decimal
		if (!pcWord[++nPos]) return dTag;							// We could end in a decimal

		pcPos = &(pcWord[nPos]);									// Eat decimal digits
		while (!strcspn(pcPos,"0123456789")) {
			nPos++;
			if (!*(++pcPos)) return dTag;							// Ran off the end of the string so therefore legit float
		}
	}
	if (pcWord[nPos] != 'd' && pcWord[nPos] != 'D' && pcWord[nPos] != 'e' && pcWord[nPos] != 'E') return UNKNOWN; // Should have an exponential character if we aren't digits to the end
	nPos++;
	if (pcWord[nPos] == '+' || pcWord[nPos] == '-') nPos++;			// Check sign
	if (!pcWord[nPos]) return UNKNOWN;								// Can't end in a sign

	pcPos = &(pcWord[nPos]);										// Eat exponent digits
	while (!strcspn(pcPos,"0123456789")) {
		nPos++;
		if (!*(++pcPos)) return dTag;								// Ran off the end of the string so therefore legit float in scientific notation
	}
	return UNKNOWN;													// That should have been it!
}



// ============== CAssoc =========================
void CAssoc::AVLDestroy(const typeEntry *const &pNode) {

	// This function recusivly deletes the AVL tree.  It deletes the data which will call the CVariable destructor and
	// it deletes memory allocated for the Key.  It uses the same logic as the binary tree traverse algorithm used in
	// the print function.  It takes as an argument the current parameter to delete.

	if (!pNode) return;
	AVLDestroy(pNode->pLeft);
	AVLDestroy(pNode->pRight);
	delete pNode->pLeft;
	delete pNode->pRight;
	delete[] pNode->pszKey;
	delete pNode->varData;
}


const CVariable *CAssoc::AVLGet (const typeEntry *const pNode, const char *Key) const {

	// This function gets a bit of data from the tree.  It uses a standard recursive binary tree search algorithm
	// which should run in O(lg(n)) time because it is a balanced tree. It takes as arguments the current node to
	// search, and the key to look for.

	if (!pNode)
		return NULL;

	if (strcmp(Key, pNode->pszKey) < 0)
		return AVLGet(pNode->pLeft, Key);

	if (strcmp(Key, pNode->pszKey) > 0)
		return AVLGet(pNode->pRight, Key);

	return (pNode->varData);
}

void CAssoc::AVLPrint(const typeEntry *const pNode, int depth, std::ostream &s) const {

	// This prints all entries in the binary tree.  This is used mostly for debugging purposes.  It uses a standard
	// recursive tree walking algorithm and therefore prints the tree in order.  It is trivial to modify the function
	// to print in a different order. It takes as arguments, the current node to print, the current depth in the tree
	// and a pointer to a stream.

	if (!pNode) return;
	AVLPrint(pNode->pLeft, depth+1, s);
	s.width(20);
	std::cout << pNode->pszKey << *pNode->varData << std::endl;
	AVLPrint(pNode->pRight, depth+1, s);
}

CAssoc::AVLRes CAssoc::AVLPut (typeEntry *&pNode, typeEntry *const pInsert) {

	// This function adds a bit of data to tree.  It uses an algorithm from http://purists.org which is designed to
	// be understandable, but not highly optimized.  However, it looks not bad to me, and it _is_ understandable. It
	// takes as parameters the current node to search for insertine and a pointer to the element to insert.  It
	// returns a state flag indicating if further balancing is needed.

	AVLRes avlTmp;

	if (!pNode) {
		pNode = pInsert;
		return BALANCE;
	}

	if (strcmp(pInsert->pszKey, pNode->pszKey) < 0) {
		if ((avlTmp = AVLPut(pNode->pLeft, pInsert)) == BALANCE) {
			return AVLGrownLeft(pNode);
		}
		return avlTmp;
	}

	if (strcmp(pInsert->pszKey, pNode->pszKey) > 0) {
		if ((avlTmp = AVLPut(pNode->pRight, pInsert)) == BALANCE) {
			return AVLGrownRight(pNode);
		}
		return avlTmp;
	}

	if (strcmp(pInsert->pszKey, pNode->pszKey) == 0)
		throw Exception(Exception::KEYDUP,"Duplicate Key found",pNode->pszKey);

	throw Exception(Exception::UNKNOWN,"Unknown error found in AVLPut",pNode->pszKey);
}

void CAssoc::AVLRotLeft(typeEntry *&pNode) {

	// Rotate the tree to the left on the current node.  It takes a pointer to a node as the 
	// current node.

	typeEntry *entTmp = pNode;

	pNode = pNode->pRight;
	entTmp->pRight = pNode->pLeft;
	pNode->pLeft = entTmp;
}
void CAssoc::AVLRotRight(typeEntry *&pNode) {

	// Rotate the tree to the right on the current node.  It takes a pointer to a node as the 
	// current node.

	typeEntry *entTmp = pNode;

	pNode = pNode->pLeft;
	entTmp->pLeft = pNode->pRight;
	pNode->pRight = entTmp;
}

CAssoc::AVLRes CAssoc::AVLGrownLeft(typeEntry *&pNode) {

	// Called after an entry to the left.  Checks to see if we need to balance the tree and executes this
	// if necessary.  Takes a pointer to the node currently being evaluated.

	switch (pNode->nSkew) {
	case LEFT:
		if (pNode->pLeft->nSkew == LEFT) {
			pNode->nSkew = pNode->pLeft->nSkew = NONE;
			AVLRotRight(pNode);
		} else {
			switch (pNode->pLeft->pRight->nSkew) {
			case LEFT:
				pNode->nSkew = RIGHT;
				pNode->pLeft->nSkew = NONE;
				break;
			case RIGHT:
				pNode->nSkew = NONE;
				pNode->pLeft->nSkew = LEFT;
				break;
			case NONE:
				pNode->nSkew = NONE;
				pNode->pLeft->nSkew = NONE;
				break;
			}
			pNode->pLeft->pRight->nSkew = NONE;
			AVLRotLeft (pNode->pLeft);
			AVLRotRight (pNode);
		}
		return OK;

	case RIGHT:
		pNode->nSkew = NONE;
		return OK;

	case NONE:
		pNode->nSkew = LEFT;
		return BALANCE;

	default : throw Exception(Exception::UNKNOWN,"Unexpected Skew value in AVLGrownLeft",pNode->pszKey);
	}
}
CAssoc::AVLRes CAssoc::AVLGrownRight(typeEntry *&pNode) {

	// Called after an entry to the right.  Checks to see if we need to balance the tree and executes this
	// if necessary.  Takes a pointer to the node currently being evaluated.

	switch (pNode->nSkew) {
	case LEFT:
		pNode->nSkew = NONE;
		return OK;

	case RIGHT:
		if (pNode->pRight->nSkew == RIGHT) {
			pNode->nSkew = pNode->pRight->nSkew = NONE;
			AVLRotLeft (pNode);
		} else {
			switch (pNode->pRight->pLeft->nSkew) {
			case RIGHT:
				pNode->nSkew = LEFT;
				pNode->pRight->nSkew = NONE;
				break;

			case LEFT:
				pNode->nSkew = NONE;
				pNode->pRight->nSkew = RIGHT;
				break;

			case NONE:
				pNode->nSkew = NONE;
				pNode->pRight->nSkew = NONE;
				break;
			}
			pNode->pRight->pLeft->nSkew = NONE;
			AVLRotRight(pNode->pRight);
			AVLRotLeft(pNode);
		}
		return OK;

	case NONE:
		pNode->nSkew = RIGHT;
		return BALANCE;

	default : throw Exception(Exception::UNKNOWN,"Unexpected Skew value in AVLGrownRight",pNode->pszKey);
	}
}


// ============== CVariable =========================
CVariable::~CVariable () {
	// Destructor for CVariable class.  Delete any data in the pointers.

	switch (tag) {
	case pnTag :
		delete[] pn;
		break;
	case pdTag :
		delete[] pd;
		break;
	case pbTag :
		delete[] pb;
		break;
	case sTag :
		delete[] s;
		break;
	case psTag :
		for (int i = 0; i < nSize; i++)
			delete ps[i];															// ps[] will get deleted when the object is deleted - we can't delete it here because it is const
		break;
	//No mem management needed.
	case bTag : break; 
	case dTag : break; 
	case nTag : break; 
	case UNKNOWN : break; 
	}
}

const int CVariable::nVal () const {

	// A function to return the integer data.  The tag is checked and if the type is not integer, typecasting 
	// is used to return a value if possible.

	switch (tag) {
	case nTag:
		return n;
	case pnTag :
		return pn[0];
	case dTag :
		return (long) d;
	case pdTag :
		return (long) pd[0];
	case bTag :
		return (long) b;
	case pbTag :
		return (long) pb[0];
	default:																	// Don't do anything for string types
		throw Exception (Exception::BADVARTYPE,"Unable to handle string types for integer key retreival");
		return 0;
	}
}

const int CVariable::pnVal(const int *&pnArg) const {

	// A function to return the integer array data.  The tag is checked and if the type is not an integer array,
	// a size of zero is returned and the NULL pointer

	if (tag == pnTag || nSize == 0) {											// If there isn't any data, we can't be sure if it was intended as integer or real data
		pnArg = pn;
		return nSize;
	}
	throw Exception (Exception::BADVARTYPE,"Invalid data type, expected array of integers");
	return 0;
}

const double CVariable::dVal() const {

	// A function to return the double data.  The tag is checked and if the type is not double, typecasting 
	// is used to return a value if possible.

	switch (tag) {
	case nTag:
		return (double) n;
		break;
	case pnTag :
		return (double) pn[0];
		break;
	case dTag :
		return d;
		break;
	case pdTag :
		return pd[0];
		break;
	case bTag :
		return (double) b;
		break;
	case pbTag :
		return (double) pb[0];
		break;
	default:																	// Don't do anything for string types
		throw Exception (Exception::BADVARTYPE,"Unable to handle string types for double key retreival");
		return 0.;
	}
}

const int CVariable::pdVal(const double *&pdArg) const {

	// A function to return the double array data.  The tag is checked and if the type is not a double array,
	// a size of zero is returned and the NULL pointer.

	if (tag == pdTag || nSize == 0) {
		pdArg = pd;
		return nSize;
	}
	throw Exception (Exception::BADVARTYPE,"Invalid data type, expected array of doubles");
	return 0;
}

const bool CVariable::bVal() const {

	// A function to return the boolean data.  The tag is checked and if the type is not boolean, typecasting 
	// is used to return a value if possible.

	switch (tag) {
	case nTag:
		if (n == 0)
			return false;
		else
			return true;
		break;
	case pnTag :
		if (pn[0] == 0)
			return false;
		else
			return true;
		break;
	case dTag :
		if (d == 0.)
			return false;
		else
			return true;
		break;
	case pdTag :
		if (pd[0] == 0)
			return false;
		else
			return true;
		break;
	case bTag :
		return b;
		break;
	case pbTag :
		return pb[0];
		break;
	default:																	// Don't do anything for string types
		throw Exception (Exception::BADVARTYPE,"Unable to handle string types for boolean key retreival");
		return 0.;
	}
}

const int CVariable::pbVal(const bool*&pbArg) const {

	// A function to return the boolean array data.  The tag is checked and if the type is not a boolean array,
	// a size of zero is returned and the NULL pointer.

	if (tag == pbTag || nSize == 0) {
		pbArg = pb;
		return nSize;
	}
	throw Exception (Exception::BADVARTYPE,"Invalid data type, expected array of booleans");
	return 0;
}

const char *CVariable::sVal() const {

	// A function to return the string data.  The tag is checked and if the type is not string,
	// the NULL pointer is returned.

	if (tag == sTag) {
		return s;
	}
	if (tag == psTag) {
		return ps[0];
	}
	throw Exception (Exception::BADVARTYPE,"Invalid data type, expected a string");
	return NULL;
}

const int CVariable::psVal(char *const*&psArg) const {

	// A function to return the array of string data.  The tag is checked and if the type is
	// not a pointer to string the NULL pointer is returned.

	if (tag == psTag || nSize == 0) {
		psArg = ps;
		return nSize;
	}
	throw Exception (Exception::BADVARTYPE,"Invalid data type, expected array of strings");
	return NULL;
}

std::ostream& operator<< (std::ostream &s, const CVariable &varOut) {

	// This overload function outputs the a dynamic type variable to the specified stream.  It writes all the data
	// associated with the variable including the tag, the size and the data.  It returns the stream as per convention.

	s << ": Tag=" << varOut.tag << "\t";
	int nSize = 1;
	switch (varOut.tag) {
	case nTag :
		s << "Size=" << nSize << "\tData=" << varOut.nVal();
		break;

	case pnTag :
		const int *pn;
		nSize = varOut.pnVal(pn);
		s << "Size=" << nSize << "\tData=";
		for (int i = 0; i < nSize; i++)
			s << pn[i] << ", ";
		break;

	case dTag :
		s << "Size=" << nSize << "\tData=" << varOut.dVal();
		break;

	case pdTag : 
		const double *pd;
		nSize = varOut.pdVal(pd);
		s << "Size=" << nSize << "\tData=";
		for (int i = 0; i < nSize; i++)
			s << pd[i] << ", ";
		break;

	case bTag :
		s << "Size=" << nSize << "\tData=" << varOut.bVal();
		break;

	case pbTag : 
		const bool *pb;
		nSize = varOut.pbVal(pb);
		s << "Size=" << nSize << "\tData=";
		for (int i = 0; i < nSize; i++)
			s << pb[i] << ", ";
		break;

	case sTag :
		const char *st;
		st = varOut.sVal();
		s << st;
		break;

	case psTag : 
		char *const*ps;
		nSize = varOut.psVal(ps);
		s << "Size=" << nSize << "\tData=";
		for (int i = 0; i < nSize; i++)
			s << ps[i] << ", ";
		break;

	default : s << "Unknown Tag";
	}
	return s;
}