/*
 * Profiling namelist element for GENESIS4.
 *
 * Christoph Lechner, European XFEL GmbH, 13-Apr-2021
 */
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <time.h>
#include "mpi.h"
#include "Prof.h"

using std::map;
using std::pair;
using std::setw;
using std::sort;
using std::string;
using std::vector;
using std::cout;
using std::endl;

Prof::Prof()
{
	mytime_t t;

	getnano(&t);

	t0_          = t;
	times_["t0"] = t;
}
Prof::~Prof() {}

/* copy of StringProcessing::atob (not including this class because of "using namespace std" in header file, avoiding this may be considered good practice) */
bool Prof::atob(string in){
	bool ret=false;
	if ((in.compare("1")==0)||(in.compare("true")==0)||(in.compare("t")==0)) { ret=true; }
	return ret;
}

int Prof::getnano(mytime_t *outnano)
{
	mytime_t nano;
	struct timespec ts;

	if(clock_gettime(CLOCK_REALTIME, &ts)<0) {
		cout << "call to clock_gettime failed!" << endl;
		return(-1);
	}

	nano  = ts.tv_sec;
	nano *= 1e9;
	nano += ts.tv_nsec;

	*outnano = nano;
	return(0);
}

void Prof::usage(void)
{
	
}

bool Prof::report_cmp(const std::pair<std::string, mytime_t> &p1, const std::pair<std::string, mytime_t> &p2)
{
	if(p1.second < p2.second)
		return(true);
	else if(p1.second == p2.second)
		if(p1.first < p2.first)
			return(true);

	return(false);
}
void Prof::report_core(FILE *fout, bool pretty)
{
	vector<pair<string,mytime_t>> v;
	vector<string> col_labels;
	size_t max_len_label;
	vector<string> col_t1;
	size_t max_len_col1;
	vector<string> col_t2;
	size_t max_len_col2;
	vector<string> col_t3;
	size_t max_len_col3;

	/* sort data in map according to time */
	for(map<string,mytime_t>::const_iterator it = times_.begin();
	    it != times_.end();
	    ++it)
	{
		pair<string,mytime_t> p;
		p.first = it->first;
		p.second = it->second;
		v.push_back(p);
	}
	sort(v.begin(), v.end(), report_cmp);

	if(pretty==false)
	{
		for(int i=0; i<v.size(); i++)
		{
			double deltat=0.;
			if(i>0) {
				deltat = v[i].second-v[i-1].second;
			}
			fprintf(fout, "%s,%.6f,%.6f,%.6f\n",
				v[i].first.c_str(), v[i].second/1e9, (v[i].second-t0_)/1e9, deltat/1e9);
		}
		return;
	}

	/* first round prepares pretty-printing => determine needed column widths */
	string col_hdr_label("label");
	string col_hdr1("t [s]");
	string col_hdr2("t-t0 [s]");
	string col_hdr3("deltat [s]");
	max_len_label = col_hdr_label.length();
	max_len_col1 = col_hdr1.length();
	max_len_col2 = col_hdr2.length();
	max_len_col3 = col_hdr3.length();
	for(int i=0; i<v.size(); i++)
	{
		const int buflen=1000;
		char buf[buflen];
		size_t len_this;
		string tmpstr;

		len_this = v[i].first.length();
		max_len_label = (len_this > max_len_label) ? len_this : max_len_label;
		col_labels.push_back(v[i].first);

		snprintf(buf, buflen, "%.6f", v[i].second/1e9);
		tmpstr = buf;
		max_len_col1 = (tmpstr.length() > max_len_col1) ? tmpstr.length() : max_len_col1;
		col_t1.push_back(tmpstr);

		snprintf(buf, buflen, "%.6f", (v[i].second-t0_)/1e9);
		tmpstr = buf;
		max_len_col2 = (tmpstr.length() > max_len_col2) ? tmpstr.length() : max_len_col2;
		col_t2.push_back(tmpstr);

		if(i>0) {
			snprintf(buf, buflen, "%.6f", (v[i].second-v[i-1].second)/1e9);
			tmpstr = buf;
		} else {
			/* no deltat in first row */
			tmpstr = "";
		}
		max_len_col3 = (tmpstr.length() > max_len_col3) ? tmpstr.length() : max_len_col3;
		col_t3.push_back(tmpstr);
	}

	string sep1(max_len_label, '=');
	string sep2(max_len_col1, '=');
	string sep3(max_len_col2, '=');
	string sep4(max_len_col3, '=');
	fprintf(fout, "%-*s   %-*s   %-*s   %-*s\n",
		static_cast<int>(max_len_label), col_hdr_label.c_str(),
	        static_cast<int>(max_len_col1),  col_hdr1.c_str(),
	        static_cast<int>(max_len_col2),  col_hdr2.c_str(),
	        static_cast<int>(max_len_col3),  col_hdr3.c_str());
	fprintf(fout, "%s   %s   %s   %s\n", sep1.c_str(), sep2.c_str(), sep3.c_str(), sep4.c_str());
	for(int i=0; i<v.size(); i++)
	{
		fprintf(fout, "%-*s   %*s   %*s   %*s\n",
			static_cast<int>(max_len_label), col_labels[i].c_str(),
		        static_cast<int>(max_len_col1),  col_t1[i].c_str(),
		        static_cast<int>(max_len_col2),  col_t2[i].c_str(),
		        static_cast<int>(max_len_col3),  col_t3[i].c_str());
	}
	fprintf(fout, "%s   %s   %s   %s\n", sep1.c_str(), sep2.c_str(), sep3.c_str(), sep4.c_str());
}
void Prof::report(void)
{
	report_core(stdout, true);
}

bool Prof::init(int mpirank, int mpisize, map<string,string> *arg)
{
	bool do_barrier;
	bool do_record;
	string id_label;
	bool do_write;
	string fn;
	map<string,string>::iterator end=arg->end();

	do_barrier = do_record = do_write = false;
	if(arg->find("barrier") != end) {
		do_barrier = atob(arg->at("barrier"));
		arg->erase(arg->find("barrier"));
	}
	if(arg->find("label") != end) {
		id_label = arg->at("label");
		arg->erase(arg->find("label"));
		do_record = true;
	}
	if(arg->find("writetofile") != end) {
		fn = arg->at("writetofile");
		arg->erase(arg->find("writetofile"));
		do_write = true;
	}
	
	/* if only known/valid arguments were supplied, then 'arg' should be empty now... */
	if(arg->size())
	{
		if(mpirank==0)
		{
			cout << "*** Error unknown elements in &profile" << endl;
			usage();
		}
		return false;
	}


	if(do_barrier)
	{
		MPI_Barrier(MPI_COMM_WORLD);
	}

	if(do_record)
	{
		mytime_t t;

		getnano(&t);
		times_[id_label] = t;
	}

	if((do_write) && (mpirank==0))
	{
		FILE *fout;

		if(NULL==(fout=fopen(fn.c_str(), "w")))
		{
			cout << "Profiling: cannot open file " << fn << " for writing (ignoring)" << endl;
			return true;
		}
		report_core(fout, false);
		fclose(fout);
	}

	return true;
}
