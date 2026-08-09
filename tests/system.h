#include "rtl.h"
#include <functional>

using t_boolean = pas::t_boolean;

struct t_tobject {
struct m_meta {
	public: inline static ::pas::t_tclass* p_classtype() {
		inline static m_meta meta{};
		return &meta;
	}
	virtual pas::t_shortstring p_classname();
	virtual  p_classparent();
	virtual  p_classtype();
	virtual ~t_tobject();
	virtual pas::t_boolean p_inheritsfrom( p_klass);
};
	virtual pas::t_shortstring p_classname();
	virtual  p_classparent();
	virtual  p_classtype();
	virtual ~t_tobject();
	virtual pas::t_boolean p_inheritsfrom( p_klass);
};
