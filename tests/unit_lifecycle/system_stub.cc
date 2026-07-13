#include "system.h"

// The generated System unit currently leaves its external TObject.ClassType
// method undefined. Lifecycle tests link generated unit translation units, so
// provide that unrelated missing method until class externals supply it.
t_tobject::m_meta* t_tobject::m_meta::p_classtype() {
	return m_meta_instance();
}
