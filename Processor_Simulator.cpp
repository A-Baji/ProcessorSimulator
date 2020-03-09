#include <iostream>
#include <iterator>
#include <queue>
#include <list>
#include <vector>
#include <algorithm>

using namespace std;

struct instruction {
	string type;
	int t;
};

struct process {
	int pid;
	list<instruction> instr;
};

struct reqEvent {
	int pid;
	instruction instr;
};

struct compEvent {
	int pid;
	instruction instr;
	int compTime;
};


struct pTable {
	int pid;
	int sTime; 					//Start time
	int fLine;					//First line
	int lLine; 					//Last line
	int cLine; 					//Current line
	int cuCompTime; 			//Completion time of current instruction
	string qType; 				//Queue type: NI or I
	string state = "INACTIVE"; 	//Process state: RUNNING, READY, BLOCKED, or TERMINATED
};

int unCores; 			//Number of unused cores
bool ssd; 				//Keeps track of whether the SSD is in use
int timer = 0; 			//Universal Timer
int procsCompleted = 0; //Number of processes completed
int ssdReqs = 0; 		//Number of times SSD was requested
float ssdTime = 0; 		//Time spent in SSD
float coreTime = 0; 	//Time spent in core

vector<reqEvent> iQ; 	//Interactive Queue
vector<reqEvent> niQ; 	//Non Interactive Queue
vector<reqEvent> ssdQ; 	//SSD Queue

vector<pTable> processTable; 							//Keeps track of state of each process
vector<process> processList; 							//List of each process
vector<reqEvent> *reqList = new vector<reqEvent>; 		//List of request events to be executed
vector<compEvent> *compList = new vector<compEvent>; 	//List of completion events to be executed

void init() {
	string s;
	int startCount = 0;
	int line = 0;

	instruction currInst;
	process tempProcesses;
	pTable tempPT;

	while (cin >> s) {
		if (s == "START") { 
			if (startCount == 0)
				tempPT.fLine = line;
			startCount++; //Increment start counter used to establish each process
			if (startCount == 2) { //Push new process into process list and process table
				tempPT.lLine = line - 1;
				tempPT.cLine = tempPT.fLine;
				processTable.push_back(tempPT);
				processList.push_back(tempProcesses);
				tempProcesses.instr.clear();
				startCount = 1;
			}
		}

		if (s == "NCORES") { //Store # of cores
			cin >> s;
			unCores = stoi(s);
		}
		else if (s == "PID") { //Store PID
			cin >> s;
			tempPT.pid = stoi(s);
			tempProcesses.pid = stoi(s);
		}
		else if (s == "END") { //Store last process and break
			tempPT.lLine = line - 1;
			tempPT.cLine = tempPT.fLine;
			processTable.push_back(tempPT);
			processList.push_back(tempProcesses);
			tempProcesses.instr.clear();
			break;
		}
		else { //Store instructions
			currInst.type = s;
			if (s == "START") {
				tempPT.fLine = line;
				cin >> s;
				line++;
				tempPT.sTime = stoi(s);
				currInst.t = stoi(s);
				tempProcesses.instr.push_back(currInst);
			}
			else {
				cin >> s;
				line++;
				currInst.t = stoi(s);
				tempProcesses.instr.push_back(currInst);
			}
		}
	}
}

vector<pTable>::iterator getProcessFromTable(int id) { //Returns the process of given id
	for (vector<pTable>::iterator it = processTable.begin(); it != processTable.end(); ++it)
		if (id == it->pid)
			return it;
}

bool Compare(const compEvent& a, const compEvent& b) { //Compare completion times to sort completion list
	if (a.compTime < b.compTime)
		return true;
	else
		return false;
}

int getLowestReadyTime() { //Returns the earliest completion time of processes in the core. Used to calculate completion times of processes in I and NI queues
	int lowestReadyTime;
	vector<int> readyTime; 
	for (vector<pTable>::iterator it = processTable.begin(); it != processTable.end(); ++it) {
		if (it->state == "RUNNING")
			readyTime.push_back(it->cuCompTime);
		sort(readyTime.begin(),readyTime.end());
	}
	lowestReadyTime = readyTime.front();
	return lowestReadyTime;
}

void scheduleStarts() { //Schedule all process starts
	compEvent temp;
	for (vector<process>::iterator it = processList.begin(); it != processList.end(); ++it) {
		temp.pid = it->pid;
		temp.instr = it->instr.front();
		temp.compTime = temp.instr.t;
		it->instr.pop_front();
		compList->push_back(temp);
	}
}

void printTable() { //Print table
	cout << "\nProcess Table:\nPID\tStart Time\tFirst Line\tLast Line\tCurr Line\tState\n";
	for (vector<pTable>::iterator it = processTable.begin(); it != processTable.end(); ++it) 
		if (it->state != "INACTIVE")
			cout << it->pid << "\t" << it->sTime << "\t\t" << it->fLine << "\t\t" << it->lLine << "\t\t" << it->cLine << "\t\t" << it->state << "\n";
}

void termin(int id) { //Terminated a process
	cout << "\nProcess " << id << " terminates at time " << timer << " ms";
	getProcessFromTable(id)->state = "TERMINATED";
	for (vector<process>::iterator it = processList.begin(); it != processList.end(); ++it)
		if (it->pid == id)
			processList.erase(it);
	printTable();
	procsCompleted++;
}

void processNextReq(int id) { //Pop the next instruction from process of id and push to event list
	reqEvent temp;
	for (vector<process>::iterator it = processList.begin(); it != processList.end(); ++it)
		if (it->pid == id) {
			if (it->instr.size() == 0) //If no instructions left, terminate process
				termin(id);
			else {
				temp.pid = id;
				temp.instr = it->instr.front();			//Get next instruction
				it->instr.pop_front();					//Pop it from instruction list
				getProcessFromTable(it->pid)->cLine++;	//Increment current line
				reqList->push_back(temp);				//Push back instruction to request list
			}
		}
}

void coreComplete(compEvent ce) { //Release RUNNING process from core
	timer = ce.compTime;
	processNextReq(ce.pid); //Process next instruction: SSD or TTY
	if (!iQ.empty()) { //If a process is READY in I queue, move to core
		getProcessFromTable(iQ.front().pid)->state = "RUNNING";
		iQ.erase(iQ.begin());
	}
	else if (!niQ.empty()) { //If a process is READY in NI queue, move to core
		getProcessFromTable(niQ.front().pid)->state = "RUNNING";
		niQ.erase(iQ.begin());
	}
	else
		unCores++;
}

void coreReq(reqEvent re) { //Process core request
	coreTime += re.instr.t;
	if (unCores > 0) { //No process in core
		unCores--;
		getProcessFromTable(re.pid)->state = "RUNNING";
		getProcessFromTable(re.pid)->cuCompTime = timer + re.instr.t;
	}
	else { //Core in use, push into queue
		int currNiqStartTime = 0; 	//Used to get start time of process pushed to NI queue
		int currIqStartTime = 0;	//Used to get start time of process pushed to I queue
		int totalIqCompTime = 0;	//Total completion time of I queue
		totalIqCompTime = getLowestReadyTime(); 									//Set total IQ completion time to earliest release time of processes in cpu
		if (!iQ.empty())															//Check if IQ isn't empty
			for (vector<reqEvent>::iterator it = iQ.begin(); it != iQ.end(); ++it) 	//Get total IQ completion time
				totalIqCompTime += it->instr.t;
		if (getProcessFromTable(re.pid)->qType == "I") {
			currIqStartTime = getLowestReadyTime();										//Used to set completion time of process before pushing into I queue
			for (vector<reqEvent>::iterator it = iQ.begin(); it != iQ.end(); ++it) { 	//Get current process completion time: Earliest release time of cpu + execution time of every process in front of it in the queue
				if (it->pid == re.pid)
					break;
				currIqStartTime += it->instr.t;
			}
			iQ.push_back(re); //Push process into IQ
			getProcessFromTable(re.pid)->cuCompTime = currIqStartTime + re.instr.t; //Set completion time of process: Start time + execution time
			getProcessFromTable(re.pid)->state = "READY";
		}
		else {
			currNiqStartTime = totalIqCompTime; 										//Set start time of current process to the total completion time of I queue
			for (vector<reqEvent>::iterator it = niQ.begin(); it != niQ.end(); ++it) { 	//Get current process start time: Earliest release time of cpu + total I queue time + execution time of every process in front of it in the queue
				if (it->pid == re.pid)
					break;
				currNiqStartTime += it->instr.t;
			}
			niQ.push_back(re); //Push process into NIQ
			getProcessFromTable(re.pid)->cuCompTime = currNiqStartTime + re.instr.t; //Set completion time of process: Start time + execution time
			getProcessFromTable(re.pid)->state = "READY";
		}
	}
	compEvent temp; 											//{
	temp.pid = re.pid;											//
	temp.instr = re.instr;										//
	temp.compTime = getProcessFromTable(re.pid)->cuCompTime;	//
	compList->push_back(temp); 									//} Push instruction to completion list to be executed at set completion time
	sort(compList->begin(), compList->end(), Compare);			//Sort completion list by earliest completion time
}

void ssdComplete(compEvent ce) { //Release BLOCKED process from SSD
	timer = ce.compTime;
	getProcessFromTable(ce.pid)->qType == "NI"; //Set process to NI
	processNextReq(ce.pid); //Process next instruction: CORE
	if (!ssdQ.empty()) { //If a process in in SSD Queue, move to SSD
		getProcessFromTable(ssdQ.front().pid)->cuCompTime = timer + ssdQ.front().instr.t; //Set process completion time
		ssdQ.erase(ssdQ.begin());
	}
	else
		ssd = true;
}

void ssdReq(reqEvent re) { //Process SSD request
	ssdReqs++;
	ssdTime+= re.instr.t;
	if (ssd) { //SSD is not in use, move process to SSD
		ssd = false;
		getProcessFromTable(re.pid)->state = "BLOCKED";
		getProcessFromTable(re.pid)->cuCompTime = timer + re.instr.t; //Set process completion time
	}
	else { //SSD in use, push into queue
		ssdQ.push_back(re);
		getProcessFromTable(re.pid)->state = "BLOCKED";
	}
	compEvent temp; 											//{
	temp.pid = re.pid;											//
	temp.instr = re.instr;										//
	temp.compTime = getProcessFromTable(re.pid)->cuCompTime;	//
	compList->push_back(temp); 									//} Push instruction to completion list to be executed at set completion time
	sort(compList->begin(), compList->end(), Compare);			//Sort completion list by earliest completion time
}

void ttyComplete(compEvent ce) {
	timer = ce.compTime;
	getProcessFromTable(ce.pid)->qType = "I";
	processNextReq(ce.pid); //Process next instruction: CORE
}

void ttyReq(reqEvent re) {
	getProcessFromTable(re.pid)->state = "BLOCKED";
	getProcessFromTable(re.pid)->cuCompTime = timer + re.instr.t; //Set completion time
	compEvent temp; 											//{
	temp.pid = re.pid;											//
	temp.instr = re.instr;										//
	temp.compTime = getProcessFromTable(re.pid)->cuCompTime;	//
	compList->push_back(temp); 									//} Push instruction to completion list to be executed at set completion time
	sort(compList->begin(), compList->end(), Compare);			//Sort completion list by earliest completion time
}

void startProcess(int id, int t) {
	reqEvent temp;
	timer = t;
	getProcessFromTable(id)->qType = "NI";
	cout << "\nProcess " << id << " starts at time " << t << " ms";
	for (vector<process>::iterator it = processList.begin(); it != processList.end(); ++it) //Process first instruction
		if (it->pid == id) {
			temp.pid = id;
			temp.instr = it->instr.front();						//Get next instruction
			it->instr.pop_front();								//Pop it from instruction list
			getProcessFromTable(it->pid)->cLine++;				//Increment current line
			reqList->insert(reqList->begin(),temp);				//Push first request of process to front of request list list		
		}
	printTable();
}

void executeRequest() { //Execute instruction at front of event list
	reqEvent temp;						//{
	temp.instr = reqList->front().instr;//
	temp.pid = reqList->front().pid;	//
	reqList->erase(reqList->begin());	//} Store next request event in temp

	if (temp.instr.type == "START")
		startProcess(temp.pid, temp.instr.t);
	else if (temp.instr.type == "CORE") {
		coreReq(temp);
	}
	else if (temp.instr.type == "SSD") {
		ssdReq(temp);
	}
	else if (temp.instr.type == "TTY") {
		ttyReq(temp);
	}
}

void executeComplete() {
	cout << "\nEXECING COMP:";
	compEvent temp;								//{
	temp.instr = compList->front().instr;		//
	temp.pid = compList->front().pid;			//
	temp.compTime = compList->front().compTime;	//
	compList->erase(compList->begin());			//} Store next completion event in temp

	if (temp.instr.type == "START")
		startProcess(temp.pid, temp.instr.t);
	else if (temp.instr.type == "CORE") {
		coreComplete(temp);
	}
	else if (temp.instr.type == "SSD") {
		ssdComplete(temp);
	}
	else if (temp.instr.type == "TTY") {
		ttyComplete(temp);
	}
}

int main()
{
	init(); 						//Initialize data structures
	scheduleStarts(); 				//Schedule start times of each process
	executeComplete();
	while (!reqList->empty()) { 	//While there are still request events
		executeRequest(); 			//Execute next request
		if (reqList->empty()) { 	//If the request list is empty
			if(!compList->empty()) 	//If the complete list isn't empty
				executeComplete();	//Execute next completion
		}
	}
	cout <<"\nSUMMARY: \nTotal elapsed time: " << timer << " ms\nNumber of completed processes: " << procsCompleted << "\nTotal number of SSD accesses: " << ssdReqs << "\nAverage number of busy cores: " << coreTime/timer << "nSSD utilization: " << ssdTime/timer << endl;

	return 0;
}