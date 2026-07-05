unit system;

interface

type 
  Byte = external nil name 'pas::t_byte';
  ShortInt = external nil name 'pas::t_shortint';
  Word = external nil name 'pas::t_word';
  SmallInt = external nil name 'pas::t_smallint';
  Cardinal = external nil name 'pas::t_cardinal';
  Integer = external nil name 'pas::t_integer';
  LongInt = external nil name 'pas::t_longint';
  QWord = external nil name 'pas::t_qword';
  Int64 = external nil name 'pas::t_int64';
  Boolean = (False, True);
  Char = external nil name 'pas::t_char';
  Double = external nil name 'pas::t_double';

operator and(a, b: Boolean): Boolean; external nil name 'pas::p_logicaland';
operator or(a, b: Boolean): Boolean; external nil name 'pas::p_logicalor';
operator xor(a, b: Boolean): Boolean; external nil name 'pas::p_logicalxor';

operator+(a, b: Cardinal): Cardinal; external nil name 'pas::p_add';
operator+(a, b: Integer): Integer; external nil name 'pas::p_add';
operator+(a, b: QWord): QWord; external nil name 'pas::p_add';
operator+(a, b: Int64): Int64; external nil name 'pas::p_add';

operator-(a, b: Cardinal): Cardinal; external nil name 'pas::p_subtract';
operator-(a, b: Integer): Integer; external nil name 'pas::p_subtract';
operator-(a, b: QWord): QWord; external nil name 'pas::p_subtract';
operator-(a, b: Int64): Int64; external nil name 'pas::p_subtract';

operator*(a, b: Cardinal): Cardinal; external nil name 'pas::p_multiply';
operator*(a, b: Integer): Integer; external nil name 'pas::p_multiply';
operator*(a, b: QWord): QWord; external nil name 'pas::p_multiply';
operator*(a, b: Int64): Int64; external nil name 'pas::p_multiply';

operator/(a, b: Cardinal): Double; external nil name 'pas::p_divide';
operator/(a, b: Integer): Double; external nil name 'pas::p_divide';
operator/(a, b: QWord): Double; external nil name 'pas::p_divide';
operator/(a, b: Int64): Double; external nil name 'pas::p_divide';

// Well, I decided all CUSTOM operators have a return type, so also this one.
operator :=(out a: Cardinal; b: Cardinal): {out} Cardinal; external nil name 'pas::p_assign';

operator <(a, b: Cardinal): Boolean; external nil name 'pas::p_lessthan';
operator <=(a, b: Cardinal): Boolean; external nil name 'pas::p_lessthanorequal';
operator =(a, b: Cardinal): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Cardinal): Boolean; external nil name 'pas::p_greaterthan';
operator >=(a, b: Cardinal): Boolean; external nil name 'pas::p_greaterthanorequal';
//operator <>(a, b: Cardinal): Boolean; external nil name 'pas::p_notequal';
operator div(a, b: Cardinal): Cardinal; external nil name 'pas::p_intdivide';
operator mod(a, b: Cardinal): Cardinal; external nil name 'pas::p_modulus';

operator and(a, b: Cardinal): Cardinal; external nil name 'pas::p_bitwiseand';
operator or(a, b: Cardinal): Cardinal; external nil name 'pas::p_bitwiseor';
operator xor(a, b: Cardinal): Cardinal; external nil name 'pas::p_bitwisexor';

operator and(a, b: Boolean): Boolean; external nil name 'pas::p_logicaland';
operator or(a, b: Boolean): Boolean; external nil name 'pas::p_logicalor';
operator xor(a, b: Boolean): Boolean; external nil name 'pas::p_logicalxor';
operator not(a: Boolean): Boolean; external nil name 'pas::p_logicalnot';

operator shl(a, b: Cardinal): Cardinal; external nil name 'pas::p_shl';
operator shr(a, b: Cardinal): Cardinal; external nil name 'pas::p_shr'; // FIXME is shl shr operand 2 a byte ?

// FIXME: function ord(const x): Cardinal; external nil name 'pas::p_ord';
// FIXME: procedure inc(var x); external nil name 'pas::p_inc';
// FIXME: procedure dec(var x); external nil name 'pas::p_dec';
// FIXME: function assigned(const x: Pointer): Boolean; external nil name 'pas::p_assigned';

implementation

end.
