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

operator and(a, b: Boolean): Boolean; external nil name 'pas::p_and';
operator or(a, b: Boolean): Boolean; external nil name 'pas::p_or';
operator xor(a, b: Boolean): Boolean; external nil name 'pas::p_xor';

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

operator <(a, b: Cardinal): Boolean; external nil name 'pas::p_less';
operator <=(a, b: Cardinal): Boolean; external nil name 'pas::p_less_equal';
operator =(a, b: Cardinal): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Cardinal): Boolean; external nil name 'pas::p_greater';
operator >=(a, b: Cardinal): Boolean; external nil name 'pas::p_greater_equal';
operator <>(a, b: Cardinal): Boolean; external nil name 'pas::p_not_equal';
operator and(a, b: Cardinal): Cardinal; external nil name 'pas::p_and';
operator div(a, b: Cardinal): Cardinal; external nil name 'pas::p_div';
operator mod(a, b: Cardinal): Cardinal; external nil name 'pas::p_mod';
operator or(a, b: Cardinal): Cardinal; external nil name 'pas::p_or';
operator xor(a, b: Cardinal): Cardinal; external nil name 'pas::p_xor';

operator shl(a, b: Cardinal): Cardinal; external nil name 'pas::p_shl';
operator shr(a, b: Cardinal): Cardinal; external nil name 'pas::p_shr'; // FIXME is shl shr operand 2 a byte ?

// FIXME: function ord(const x): Cardinal; external nil name 'pas::p_ord';
// FIXME: procedure inc(var x); external nil name 'pas::p_inc';
// FIXME: procedure dec(var x); external nil name 'pas::p_dec';
 
implementation

end.
