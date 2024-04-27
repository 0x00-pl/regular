#include <iostream>
#include <vector>
#include <string>
using namespace std;

namespace pl{namespace regular{
	class match_ret{
	public:
		struct command{
			command(){next=NULL;}
			virtual ~command(){}
			virtual void execute() =0;
			command* next;
		};
		match_ret(){}
		match_ret(bool b){state=b; head=end=NULL;}
		operator bool(){return state;}

		void add(command* cmd){
			if(end==NULL) end=cmd;
			cmd->next= head;
			head= cmd;
		}
		void link(match_ret& after){
			if(end!=NULL) end->next=after.head;
			if(after.end!=NULL) end=after.end;
		}
		void execute(){
			while(head){
				head->execute();
				end= head;//use end as temp val
				head= head->next;
				delete end;
			}
		}
		void delete_sub(){
			while(head){
				end=head;
				head=head->next;
				delete end;
			}
		}

		command* head;
		command* end;
		bool state;
	};

	class unit{
	public:
		enum{match_null=true};
		enum my_type{Terr,Tsimple_leaf,Tsub_unit,Tmilt_unit,Tor_unit};
		virtual ~unit(){}
		virtual match_ret match(const char*& p)=0;
		virtual void delete_sub()=0;
		my_type _type;
	};
	class simple_leaf: public unit{
	public:
		simple_leaf(char _c):c(_c){_type=unit::Tsimple_leaf;}
		void delete_sub(){}
		match_ret match(const char*& p){
			if(*p==c){
				++p;
				return true;
			}
			return false;
		}
		char c;
	};

	class sub_unit: public unit{
	public:
		class cmd_factory{
		public:
			virtual ~cmd_factory(){}
			virtual match_ret::command* create_cmd(sub_unit*,const char* from,const char* to)=0;
		};
		sub_unit(){_type=unit::Tsub_unit;fac=NULL;}
		sub_unit(unit* n){add(n);_type=unit::Tsub_unit;fac=NULL;}

		void delete_sub(){
			for(size_t i=0; i<sub.size(); i++){
				sub[i]->delete_sub();
				delete sub[i];
			}
			delete fac;
		}
		void add(unit* n){ 
			sub.push_back(n);
		}

		void add_factory(cmd_factory* _fac){fac=_fac;}

		match_ret match(const char*& p){
			const char* tp= p;
			match_ret r(true);
			for(size_t i=0; i< sub.size(); i++){
				match_ret mr_temp= sub[i]->match(tp);
				if(mr_temp == true)
					r.link(mr_temp);
				else{
					r.delete_sub();
					return false;
				}
			}
			if(fac) r.add( fac->create_cmd(this,p,tp) );
			p= tp;
			return r;
		}

		cmd_factory* fac;
		vector<unit*> sub;
	};

	class mult_unit: public unit{
	public:
		class cmd_factory{
		public:
			virtual ~cmd_factory(){}
			virtual match_ret::command* create_cmd(mult_unit*,size_t)=0;
		};
		mult_unit(unit* u, size_t n_beg=0, size_t n_end=999)
			:_beg(n_beg), _end(n_end){_type=unit::Tmilt_unit;fac=NULL;}
		void delete_sub(){
			mult->delete_sub();
			delete mult;
			delete fac;
		}
		void add_callback(cmd_factory* _fac){fac=_fac;}
		match_ret match(const char*& p){
			if(mult==NULL)return unit::match_null;
			const char* tp= p;
			size_t i;
			for(i= 0; i<=_beg; i++){
				if( !mult->match(tp) )
					return false;
			}
			//as success 
			for(; i<_end; i++){
				if( !mult->match(tp) )
					break;
			}
			match_ret r(true);
			if(fac) r.add( fac->create_cmd(this,i) );
			p= tp;
			return r;
		}

		unit* mult;
		size_t _beg;
		size_t _end;
		cmd_factory* fac;
	};

	class or_unit: public unit{
	public:
		or_unit(unit* l, unit* r):left(l), right(r){_type=unit::Tor_unit;}
		void delete_sub(){
			left->delete_sub();
			delete left;
			right->delete_sub();
			delete right;
		}
		match_ret match(const char*& p){
			const char* tp= p;
			if(left==NULL)return unit::match_null;
			if(left->match(tp)){
				p= tp;
				return true;
			}
			tp= p;
			if(right==NULL)return unit::match_null;
			if(right->match(tp)){
				p= tp;
				return true;
			}
			return false;
		}
		unit* left;
		unit* right;
	};
	//mult: exp{0,5} exp* exp? exp+
	//or: exp|exp [exp,exp]
	unit* builder(const char* str){
		sub_unit* ret= new sub_unit();
		while(str){
			switch(*str){
			case '\0': goto exit;
			case '\\':
				++str;
				ret->add(new simple_leaf(*str++));
				break;
			case '(':
				break;
			default:
				ret->add(new simple_leaf(*str++));
			}
		}
		exit:return ret;
	}
}
}

namespace pl{namespace regular_v2{
	class offline_func{
	public:
		virtual ~offline_func(){}
		virtual void run(){}
	};
	class rule{
	public:
		class node{
		public:
			node(rule* _rby,const char* n_at, offline_func* callback= NULL)
				:ruleby(_rby), func(callback), node_at(n_at){}
			virtual ~node(){}//delete all sub 

			void run_func(){
				if(func==NULL) return;
				func->run();
				delete func;
			}
			rule* ruleby;
			const char* node_at;
			offline_func* func;
		};
	public:
		rule(){}
		virtual node* match_next(const char*& ptr, node* n= NULL) =0;
	};

	class rule_char: public rule{
	public:
		typedef rule::node node;
		rule_char(char _c): c(_c){}

		node* match_next(const char*& ptr, node* n= NULL){
			if(n==NULL && *ptr==c)
				return new node(this,ptr);
			delete n;
			return NULL;
		}

		char c;
	};
}
}

class test_cbk: public pl::regular::sub_unit::cmd_factory{
public:
	class my_command: public pl::regular::match_ret::command{
	public:
		my_command(const char* from,const char* to){
			s=string(from,to);
		}
		void execute(){cout<<"pickup: "<<s<<endl;}
		string s;
	};
	pl::regular::match_ret::command* create_cmd(pl::regular::sub_unit* obj,const char* from,const char* to){
		return new my_command(from, to);
	}
};
void main(){
	const char* tested= "hello world";
	pl::regular_v2::rule_char rh('h');

	pl::regular_v2::rule::node* ret=rh.match_next(tested);
	cout<<(bool)ret<<endl;
	cout<<"-Fin-"<<endl;
	system("pause");
}