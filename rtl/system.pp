unit system;

interface

type 
  Byte = external nil name 'pas::t_byte';
  ShortInt = external nil name 'pas::t_shortint';
  Word = external nil name 'pas::t_word';
  SmallInt = external nil name 'pas::t_smallint';
  Cardinal = external nil name 'pas::t_longword';
  LongWord = external nil name 'pas::t_longword';
  DWord = external nil name 'pas::t_longword';
  Integer = external nil name 'pas::t_integer';
  LongInt = external nil name 'pas::t_longint';
  QWord = external nil name 'pas::t_qword';
  Int64 = external nil name 'pas::t_int64';
  Boolean = external nil name 'pas::t_boolean';
  Char = external nil name 'pas::t_char';
  Double = external nil name 'pas::t_double';
  Extended = external nil name 'pas::t_extended';
  Pointer = external nil name 'pas::t_pointer';
  PtrInt = external nil name 'pas::t_ptrint';
  PtrUInt = external nil name 'pas::t_ptruint';
  SizeInt = external nil name 'pas::t_sizeint';
  SizeUInt = external nil name 'pas::t_sizeuint';
  shortstring = external nil name 'pas::t_shortstring';
  PShortString = ^shortstring;
  PChar = ^Char;
  AnsiString = external nil name 'pas::t_ansistring';
  // `class of X` is a real class-reference type in the compiler.  The C++
  // representation is the target class's metaclass pointer: X::m_meta*.
  TClass = class of TObject;
  TObject = class
  public
    destructor Destroy; virtual;
    
    // Class methods live on the generated m_meta class.  The regular class
    // emits static proxies so TObject.ClassName and obj.ClassName both dispatch
    // through the metaclass instance.
    class function ClassType: TClass; virtual; external nil name 'p_classtype';
    class function ClassName: shortstring; virtual;
    class function InheritsFrom(klass: TClass): Boolean; virtual;
    class function ClassParent: TClass; virtual;
  end;
  
operator+(a: Cardinal): Cardinal; external nil name 'pas::p_positive';
operator+(a: Integer): Integer; external nil name 'pas::p_positive';
operator+(a: QWord): QWord; external nil name 'pas::p_positive';
operator+(a: Int64): Int64; external nil name 'pas::p_positive';
operator+(a: Double): Double; external nil name 'pas::p_positive';
operator+(a: Extended): Extended; external nil name 'pas::p_positive';

operator+(a, b: Cardinal): Cardinal; external nil name 'pas::p_add';
operator+(a, b: Integer): Integer; external nil name 'pas::p_add';
operator+(a, b: QWord): QWord; external nil name 'pas::p_add';
operator+(a, b: Int64): Int64; external nil name 'pas::p_add';
operator+(a, b: Double): Double; external nil name 'pas::p_add';
operator+(a, b: Extended): Extended; external nil name 'pas::p_add';

operator-(a: Cardinal): Cardinal; external nil name 'pas::p_negative';
operator-(a: Integer): Integer; external nil name 'pas::p_negative';
operator-(a: QWord): QWord; external nil name 'pas::p_negative';
operator-(a: Int64): Int64; external nil name 'pas::p_negative';
operator-(a: Double): Double; external nil name 'pas::p_negative';
operator-(a: Extended): Extended; external nil name 'pas::p_negative';

operator-(a, b: Cardinal): Cardinal; external nil name 'pas::p_subtract';
operator-(a, b: Integer): Integer; external nil name 'pas::p_subtract';
operator-(a, b: QWord): QWord; external nil name 'pas::p_subtract';
operator-(a, b: Int64): Int64; external nil name 'pas::p_subtract';
operator-(a, b: Double): Double; external nil name 'pas::p_subtract';
operator-(a, b: Extended): Extended; external nil name 'pas::p_subtract';

operator*(a, b: Cardinal): Cardinal; external nil name 'pas::p_multiply';
operator*(a, b: Integer): Integer; external nil name 'pas::p_multiply';
operator*(a, b: QWord): QWord; external nil name 'pas::p_multiply';
operator*(a, b: Int64): Int64; external nil name 'pas::p_multiply';
operator*(a, b: Double): Double; external nil name 'pas::p_multiply';
operator*(a, b: Extended): Extended; external nil name 'pas::p_multiply';

operator/(a, b: Cardinal): Double; external nil name 'pas::p_divide';
operator/(a, b: Integer): Double; external nil name 'pas::p_divide';
operator/(a, b: QWord): Double; external nil name 'pas::p_divide';
operator/(a, b: Int64): Double; external nil name 'pas::p_divide';
operator/(a, b: Double): Double; external nil name 'pas::p_divide';
operator/(a, b: Extended): Extended; external nil name 'pas::p_divide';

operator :=(a: Cardinal): Cardinal; external nil name 'pas::p_assign';
operator :=(a: Boolean): Boolean; external nil name 'pas::p_assign';
operator :=(a: Char): Char; external nil name 'pas::p_assign';
operator :=(a: Char): ShortString; external nil name 'pas::p_char_to_shortstring';

operator <(a, b: Char): Boolean; external nil name 'pas::p_lessthan';
operator <=(a, b: Char): Boolean; external nil name 'pas::p_lessthanorequal';
operator =(a, b: Char): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Char): Boolean; external nil name 'pas::p_greaterthan';
operator >=(a, b: Char): Boolean; external nil name 'pas::p_greaterthanorequal';

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

operator shl(a, b: Cardinal): Cardinal; external nil name 'pas::p_leftshift';
operator shr(a, b: Cardinal): Cardinal; external nil name 'pas::p_rightshift'; // FIXME is shl shr operand 2 a byte ?

operator <(a, b: Int64): Boolean; external nil name 'pas::p_lessthan';
operator <=(a, b: Int64): Boolean; external nil name 'pas::p_lessthanorequal';
operator =(a, b: Int64): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Int64): Boolean; external nil name 'pas::p_greaterthan';
operator >=(a, b: Int64): Boolean; external nil name 'pas::p_greaterthanorequal';
//operator <>(a, b: Int64): Boolean; external nil name 'pas::p_notequal';
operator div(a, b: Int64): Cardinal; external nil name 'pas::p_intdivide';
operator mod(a, b: Int64): Cardinal; external nil name 'pas::p_modulus';

operator and(a, b: Int64): Cardinal; external nil name 'pas::p_bitwiseand';
operator or(a, b: Int64): Cardinal; external nil name 'pas::p_bitwiseor';
operator xor(a, b: Int64): Cardinal; external nil name 'pas::p_bitwisexor';

operator shl(a, b: Int64): Cardinal; external nil name 'pas::p_leftshift';
operator shr(a, b: Int64): Cardinal; external nil name 'pas::p_rightshift'; // FIXME is shl shr operand 2 a byte ?

operator <(a, b: Double): Boolean; external nil name 'pas::p_lessthan';
operator <=(a, b: Double): Boolean; external nil name 'pas::p_lessthanorequal';
operator =(a, b: Double): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Double): Boolean; external nil name 'pas::p_greaterthan';
operator >=(a, b: Double): Boolean; external nil name 'pas::p_greaterthanorequal';

operator <(a, b: Extended): Boolean; external nil name 'pas::p_lessthan';
operator <=(a, b: Extended): Boolean; external nil name 'pas::p_lessthanorequal';
operator =(a, b: Extended): Boolean; external nil name 'pas::p_equal';
operator >(a, b: Extended): Boolean; external nil name 'pas::p_greaterthan';
operator >=(a, b: Extended): Boolean; external nil name 'pas::p_greaterthanorequal';

function ord(const x): Cardinal; external nil name 'pas::p_ord'; // generic intrinsic
function low(const x): Integer; external nil name 'pas::p_low'; // generic intrinsic: parser supplies the type operand/result
function high(const x): Integer; external nil name 'pas::p_high'; // generic intrinsic: parser supplies the type operand/result
function length(const x): Integer; external nil name 'pas::p_length'; // generic intrinsic
procedure inc(var x; n: Integer = 1); external nil name 'pas::p_inc'; // generic intrinsic
procedure dec(var x; n: Integer = 1); external nil name 'pas::p_dec'; // generic intrinsic
procedure str(const x: Int64; var s: ShortString); overload; external nil name 'pas::p_str';
procedure str(const x: QWord; var s: ShortString); overload; external nil name 'pas::p_str';
function assigned(const x: Pointer): Boolean; external nil name 'pas::p_assigned';
function trunc(const x: Extended): Int64; external nil name 'pas::p_trunc';
function round(const x: Extended): Int64; external nil name 'pas::p_round';
function frac(const x: Extended): Extended; external nil name 'pas::p_frac';
function sqrt(const x: Extended): Extended; external nil name 'pas::p_sqrt';
function exp(const x: Extended): Extended; external nil name 'pas::p_exp';
function ln(const x: Extended): Extended; external nil name 'pas::p_ln';
function pos(const needle: ShortString; const haystack: ShortString): LongInt; overload; external nil name 'pas::p_pos';
function pos(const needle: ShortString; const haystack: AnsiString): LongInt; overload; external nil name 'pas::p_pos';
function pos(const needle: AnsiString; const haystack: AnsiString): LongInt; overload; external nil name 'pas::p_pos';
function pos(needle: Char; const haystack: ShortString): LongInt; overload; external nil name 'pas::p_pos';
procedure delete(var value: ShortString; index, count: LongInt); overload; external nil name 'pas::p_delete';
procedure delete(var value: AnsiString; index, count: LongInt); overload; external nil name 'pas::p_delete';
procedure insert(const source: ShortString; var destination: ShortString; index: LongInt); overload; external nil name 'pas::p_insert';
procedure insert(source: Char; var destination: ShortString; index: LongInt); overload; external nil name 'pas::p_insert';
procedure insert(const source: AnsiString; var destination: AnsiString; index: LongInt); overload; external nil name 'pas::p_insert';

operator xor(a, b: Boolean): Boolean; external nil name 'pas::p_logicalxor';
operator not(a: Boolean): Boolean; external nil name 'pas::p_logicalnot';

operator =(a, b: shortstring): Boolean; external nil name 'pas::p_equal';
operator :=(a: shortstring): ansistring; external nil name 'pas::p_assign';

procedure SetLength(var destination: AnsiString; value: LongInt); external nil name 'pas::p_setlength';
procedure UniqueString(var value: AnsiString); external nil name 'pas::p_uniquestring';

implementation

destructor TObject.Destroy;
begin
end;

class function TObject.ClassName: shortstring;
begin
  Result := ClassType().ClassName
end;

class function TObject.InheritsFrom(klass: TClass): Boolean;
begin
  Result := ClassType().InheritsFrom(klass)
end;

class function TObject.ClassParent: TClass;
begin
  Result := ClassType().ClassParent
end;

end.
