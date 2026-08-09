#include "system.h"
#include "rtl.h"
#include <functional>


t_tobject::~t_tobject() {
}

pas::t_shortstring t_tobject::p_classname() {
	pas::t_shortstring p_result;
	p_result = p_classtype()->p_classname();
	return p_result;
}

pas::t_boolean t_tobject::p_inheritsfrom( p_klass) {
	pas::t_boolean p_result;
	p_result = p_classtype()->p_inheritsfrom(p_klass);
	return p_result;
}

 t_tobject::p_classparent() {
	 p_result;
	p_result = p_classtype()->p_classparent();
	return p_result;
}
