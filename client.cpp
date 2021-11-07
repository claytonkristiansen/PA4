#include "common.h"
#include "FIFOreqchannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
#include <thread>
using namespace std;

enum REQUEST_TYPE {FILE_REQUEST_TYPE, DATA_REQUEST_TYPE};

void testingFunc(bool yeehaw)
{
	std::cout << "BITCH\n";
}

struct Response
{
	int m_patient;
	double m_ecg;
	Response(int patient, double ecg)
	{
		m_patient = patient;
		m_ecg = ecg;
	}
	Response()
	{
		m_patient = 0;
		m_ecg = 0;
	}
};

vector<char> CharArrToVecOfChar(char *arr, int size)
{
	vector<char> vec;
	for(int i = 0; i < size; ++i)
	{
		vec.push_back(arr[i]);
	}
	return vec;
}

vector<char> ResponseToVecOfChar(Response res)
{
	char buf[sizeof(Response) + 1];
	std::memcpy(buf, &res, sizeof(Response) + 1);
	return CharArrToVecOfChar(buf, sizeof(Response));
}

char* VecOfCharToCharArr(vector<char> vec)
{
	char arr[vec.size() + 1];
	for(int i = 0; i < vec.size(); ++i)
	{
		arr[i] = vec[i];
	}
	return arr;
}

Response VecOfCharToResponse(vector<char> vec)
{
	char* buf = VecOfCharToCharArr(vec);
	Response r;
	std::memcpy(&r, buf, vec.size());
	return r;
}



void patient_thread_function(REQUEST_TYPE rqType, BoundedBuffer *requestBuffer, int patient, int numItems, string fileName)
{
	switch(rqType)
	{
		case DATA_REQUEST_TYPE:
			for(int i = 0; i < numItems; ++i)
			{
				char buf[sizeof(DataRequest) + 1];
				DataRequest dataRequest(patient, 0.004 * i, 1);
				std::memcpy(buf, &dataRequest, sizeof(DataRequest) + 1);
				requestBuffer->push(CharArrToVecOfChar(buf, sizeof(DataRequest)));
				dataRequest = DataRequest(patient, 0.004 * i, 2);
				std::memcpy(buf, &dataRequest, sizeof(DataRequest) + 1);
				requestBuffer->push(CharArrToVecOfChar(buf, sizeof(DataRequest)));
			}
			break;
		case FILE_REQUEST_TYPE:

			break;
	}
}

void worker_thread_function(REQUEST_TYPE rqType, FIFORequestChannel &mainChan, BoundedBuffer &requestBuffer, BoundedBuffer &responseBuffer, int bufferCapacity)
{
	Request nc (NEWCHAN_REQ_TYPE);
	mainChan.cwrite (&nc, sizeof(nc));
	char nameBuf[bufferCapacity];
	mainChan.cread(nameBuf, bufferCapacity);
	FIFORequestChannel chan(string(nameBuf), FIFORequestChannel::CLIENT_SIDE);		//Request new channel
	bool done = false;
	if(rqType == DATA_REQ_TYPE)
	{
		while(!done)
		{
			vector<char> req = requestBuffer.pop();
			int reqBufSize = req.size();
			char* reqBuf = VecOfCharToCharArr(req);
			DataRequest dataReq(0, 0, 0);
			std::memcpy(&dataReq, reqBuf, sizeof(DataRequest));
			int patient = dataReq.person;

			chan.cwrite (&reqBuf, reqBufSize); //question
			Request quitRequest(QUIT_REQ_TYPE);
			if(std::memcmp(reqBuf, &quitRequest, sizeof(Request)) == 0)	//break
			{
				break;
			}
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			responseBuffer.push(ResponseToVecOfChar(Response(patient, reply)));
		}
	}
	else if(rqType == FILE_REQ_TYPE)
	{

	}
	
}
void histogram_thread_function(HistogramCollection &histogramCollection, BoundedBuffer &responseBuffer)
{
	bool done = false;
	while(!done)
	{
		Response resp = VecOfCharToResponse(responseBuffer.pop());
	}
}
int main(int argc, char *argv[])
{

	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	string filename = "";
	int b = 10; // size of bounded buffer, note: this is different from another variable buffercapacity/m
	// take all the arguments first because some of these may go to the server

	int n = 1000;
	int w = 50;
	int h = 5;
	int m = 256;
	REQUEST_TYPE reqType = DATA_REQUEST_TYPE;

	while ((opt = getopt(argc, argv, "f:n:p:w:b:h:m:")) != -1)
	{
		switch (opt)
		{
		case 'f':
			filename = optarg;
			reqType = FILE_REQUEST_TYPE;
			break;
		case 'n':
			n = stoi(optarg);
			break;
		case 'p':
			p = stoi(optarg);
			break;
		case 'w':
			w = stoi(optarg);
			break;
		case 'b':
			b = stoi(optarg);
			break;
		case 'h':
			h = stoi(optarg);
			break;
		case 'm':
			m = stoi(optarg);
			break;
		}
	}

	int pid = fork();
	if (pid < 0)
	{
		EXITONERROR("Could not create a child process for running the server");
	}
	if (!pid)
	{ // The server runs in the child process
		std::string mString = to_string(m);
		char mStr[mString.length() + 1];
		strcpy(mStr, mString.c_str());
		char *args[] = {"./server", "-m", mStr, nullptr};
		if (execvp(args[0], args) < 0)
		{
			EXITONERROR("Could not launch the server");
		}
	}
	FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
	BoundedBuffer request_buffer(b);
	BoundedBuffer response_buffer(b);
	HistogramCollection hc;

	struct timeval start, end;
	gettimeofday(&start, 0);

	/* Start all threads here */
	vector<std::thread*> patientThreads;
	vector<std::thread*> workerThreads;
	vector<std::thread*> histogramThreads;
	for(int i = 0; i < p; ++i)
	{
		//REQUEST_TYPE rqType, BoundedBuffer &requestBuffer, int patient, int numItems, string fileName
		std::thread *patientThread = new std::thread(patient_thread_function, reqType, &request_buffer, p, n, filename);
		patientThreads.push_back(patientThread);
	}
	for(std::thread* t : patientThreads)
	{
		t->join();
	}


	/* Join all threads here */
	gettimeofday(&end, 0);

	// print the results and time difference
	hc.print();
	int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
	int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
	std::cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// closing the channel
	Request q(QUIT_REQ_TYPE);
	chan.cwrite(&q, sizeof(Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	std::cout << "Client process exited" << endl;
}
