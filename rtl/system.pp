unit system;

interface

type 
  Byte = external nil name '::u_system::t_byte';
  ShortInt = external nil name '::u_system::t_shortint';
  Word = external nil name '::u_system::t_word';
  SmallInt = external nil name '::u_system::t_smallint';
  LongWord = external nil name '::u_system::t_longword';
  DWord = LongWord;
  Cardinal = LongWord;
  Integer = external nil name '::u_system::t_integer';
  LongInt = Integer;
  QWord = external nil name '::u_system::t_qword';
  Int64 = external nil name '::u_system::t_int64';
  Boolean = external nil name '::u_system::t_boolean';
  Char = external nil name '::u_system::t_char';
  Single = external nil name '::u_system::t_single';
  Double = external nil name '::u_system::t_double';
  Extended = external nil name '::u_system::t_extended';
  Pointer = external nil name '::u_system::t_pointer';
  TMethod = external nil name '::u_system::t_tmethod';
  PtrInt = external nil name '::u_system::t_ptrint';
  PtrUInt = external nil name '::u_system::t_ptruint';
  SizeInt = external nil name '::u_system::t_sizeint';
  SizeUInt = external nil name '::u_system::t_sizeuint';
  shortstring = external nil name '::u_system::t_shortstring<255>';
  Text = external nil name '::u_system::t_text';
  TextFile = Text;
  PShortString = ^shortstring;
  PChar = ^Char;
  AnsiString = external nil name '::u_system::t_ansistring';
  // `class of X` is a real class-reference type in the compiler.  The C++
  // representation is the target class's metaclass pointer: X::m_meta*.
  TClass = class of TObject;
  TObject = class
  public
    constructor Create;
    destructor Destroy; virtual;
    procedure Free; external nil name '::u_system::m_free_object';
    
    // Class methods live on the generated m_meta class. A class name supplies
    // its exact metaclass; an object supplies its dynamic metaclass through
    // the compiler-generated virtual object-to-class-reference conversion.
    class function ClassType: TClass; virtual; external nil name 'p_classtype';
    class function ClassName: shortstring; virtual;
    class function InheritsFrom(klass: TClass): Boolean; virtual;
    class function ClassParent: TClass; virtual;
    class function NewInstance: TObject; virtual;
    procedure AfterConstruction; virtual;
  end;

var
  // RunError stores its error number in this RTL variable before terminating.
  ErrorCode: Word external nil name '::u_system::p_errorcode';
  // Reset(File) consults the low two access-mode bits. Higher sharing-mode
  // bits are retained for source compatibility and ignored by this runtime.
  FileMode: Byte external nil name '::u_system::p_filemode';
  
operator+(a: Cardinal): Cardinal; external nil name '::u_system::p_positive';
operator+(a: Integer): Integer; external nil name '::u_system::p_positive';
operator+(a: QWord): QWord; external nil name '::u_system::p_positive';
operator+(a: Int64): Int64; external nil name '::u_system::p_positive';
operator+(a: Single): Single; external nil name '::u_system::p_positive';
operator+(a: Double): Double; external nil name '::u_system::p_positive';
operator+(a: Extended): Extended; external nil name '::u_system::p_positive';

operator+(a, b: Cardinal): Cardinal; external nil name '::u_system::p_add';
operator+(a, b: Integer): Integer; external nil name '::u_system::p_add';
operator+(a, b: QWord): QWord; external nil name '::u_system::p_add';
operator+(a, b: Int64): Int64; external nil name '::u_system::p_add';
operator+(a, b: Single): Single; external nil name '::u_system::p_add';
operator+(a, b: Double): Double; external nil name '::u_system::p_add';
operator+(a, b: Extended): Extended; external nil name '::u_system::p_add';

operator-(a: Cardinal): Cardinal; external nil name '::u_system::p_negative';
operator-(a: Integer): Integer; external nil name '::u_system::p_negative';
operator-(a: QWord): QWord; external nil name '::u_system::p_negative';
operator-(a: Int64): Int64; external nil name '::u_system::p_negative';
operator-(a: Single): Single; external nil name '::u_system::p_negative';
operator-(a: Double): Double; external nil name '::u_system::p_negative';
operator-(a: Extended): Extended; external nil name '::u_system::p_negative';

operator-(a, b: Cardinal): Cardinal; external nil name '::u_system::p_subtract';
operator-(a, b: Integer): Integer; external nil name '::u_system::p_subtract';
operator-(a, b: QWord): QWord; external nil name '::u_system::p_subtract';
operator-(a, b: Int64): Int64; external nil name '::u_system::p_subtract';
operator-(a, b: Single): Single; external nil name '::u_system::p_subtract';
operator-(a, b: Double): Double; external nil name '::u_system::p_subtract';
operator-(a, b: Extended): Extended; external nil name '::u_system::p_subtract';

operator*(a, b: Cardinal): Cardinal; external nil name '::u_system::p_multiply';
operator*(a, b: Integer): Integer; external nil name '::u_system::p_multiply';
operator*(a, b: QWord): QWord; external nil name '::u_system::p_multiply';
operator*(a, b: Int64): Int64; external nil name '::u_system::p_multiply';
operator*(a, b: Single): Single; external nil name '::u_system::p_multiply';
operator*(a, b: Double): Double; external nil name '::u_system::p_multiply';
operator*(a, b: Extended): Extended; external nil name '::u_system::p_multiply';

operator/(a, b: Cardinal): Double; external nil name '::u_system::p_divide';
operator/(a, b: Integer): Double; external nil name '::u_system::p_divide';
operator/(a, b: QWord): Double; external nil name '::u_system::p_divide';
operator/(a, b: Int64): Double; external nil name '::u_system::p_divide';
operator/(a, b: Single): Single; external nil name '::u_system::p_divide';
operator/(a, b: Double): Double; external nil name '::u_system::p_divide';
operator/(a, b: Extended): Extended; external nil name '::u_system::p_divide';

operator :=(a: Cardinal): Cardinal; external nil name '::u_system::p_assign';
operator :=(a: Boolean): Boolean; external nil name '::u_system::p_assign';
operator :=(a: Char): Char; external nil name '::u_system::p_assign';
operator :=(a: Char): ShortString; external nil name '::u_system::p_char_to_shortstring';

operator <(a, b: Char): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Char): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Char): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Char): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Char): Boolean; external nil name '::u_system::p_greaterthanorequal';

operator <(a, b: Cardinal): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Cardinal): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Cardinal): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Cardinal): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Cardinal): Boolean; external nil name '::u_system::p_greaterthanorequal';
//operator <>(a, b: Cardinal): Boolean; external nil name '::u_system::p_notequal';
operator div(a, b: Cardinal): Cardinal; external nil name '::u_system::p_intdivide';
operator mod(a, b: Cardinal): Cardinal; external nil name '::u_system::p_modulus';

operator and(a, b: Cardinal): Cardinal; external nil name '::u_system::p_bitwiseand';
operator or(a, b: Cardinal): Cardinal; external nil name '::u_system::p_bitwiseor';
operator xor(a, b: Cardinal): Cardinal; external nil name '::u_system::p_bitwisexor';

operator shl(a, b: Cardinal): Cardinal; external nil name '::u_system::p_leftshift';
operator shr(a, b: Cardinal): Cardinal; external nil name '::u_system::p_rightshift'; // FIXME is shl shr operand 2 a byte ?

operator <(a, b: Int64): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Int64): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Int64): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Int64): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Int64): Boolean; external nil name '::u_system::p_greaterthanorequal';
//operator <>(a, b: Int64): Boolean; external nil name '::u_system::p_notequal';
operator div(a, b: Int64): Cardinal; external nil name '::u_system::p_intdivide';
operator mod(a, b: Int64): Cardinal; external nil name '::u_system::p_modulus';

operator and(a, b: Int64): Cardinal; external nil name '::u_system::p_bitwiseand';
operator or(a, b: Int64): Cardinal; external nil name '::u_system::p_bitwiseor';
operator xor(a, b: Int64): Cardinal; external nil name '::u_system::p_bitwisexor';

operator shl(a, b: Int64): Cardinal; external nil name '::u_system::p_leftshift';
operator shr(a, b: Int64): Cardinal; external nil name '::u_system::p_rightshift'; // FIXME is shl shr operand 2 a byte ?

operator <(a, b: Single): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Single): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Single): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Single): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Single): Boolean; external nil name '::u_system::p_greaterthanorequal';

operator <(a, b: Double): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Double): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Double): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Double): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Double): Boolean; external nil name '::u_system::p_greaterthanorequal';

operator <(a, b: Extended): Boolean; external nil name '::u_system::p_lessthan';
operator <=(a, b: Extended): Boolean; external nil name '::u_system::p_lessthanorequal';
operator =(a, b: Extended): Boolean; external nil name '::u_system::p_equal';
operator >(a, b: Extended): Boolean; external nil name '::u_system::p_greaterthan';
operator >=(a, b: Extended): Boolean; external nil name '::u_system::p_greaterthanorequal';
operator in(const item; const values): Boolean; external nil name '::u_system::p_in';

function ord(const x): Cardinal; external nil name '::u_system::p_ord'; // generic intrinsic
function chr(value: Byte): Char; external nil name '::u_system::p_chr';
procedure fillchar(var destination; count: SizeInt; value: Byte); external nil name '::u_system::p_fillchar';
procedure move(const source; var destination; count: SizeInt); external nil name '::u_system::p_move';
function comparebyte(const buf1, buf2; len: SizeInt): SizeInt; external nil name '::u_system::p_comparebyte';
function comparechar(const buf1, buf2; len: SizeInt): SizeInt; external nil name '::u_system::p_comparechar';
function sizeof(const x): SizeInt; external nil name '::u_system::p_sizeof';
// Write/WriteLn have compiler grammar for a variable number of values and
// `value:width:precision`; these parameterless declarations provide normal
// name lookup and shadowing while BuiltinSyntaxKind parses the actual call.
procedure write; external nil name '::u_system::p_write';
procedure writeln; external nil name '::u_system::p_writeln';
procedure halt(value: LongInt); overload; noreturn; external nil name '::u_system::p_halt';
procedure halt; overload; noreturn; external nil name '::u_system::p_halt';
procedure runerror(value: Word); overload; noreturn; external nil name '::u_system::p_runerror';
procedure runerror; overload; noreturn; external nil name '::u_system::p_runerror';
function low(const x): Integer; external nil name '::u_system::p_low'; // generic intrinsic: parser supplies the type operand/result
function high(const x): Integer; external nil name '::u_system::p_high'; // generic intrinsic: parser supplies the type operand/result
function length(const x): SizeInt; external nil name '::u_system::p_length'; // generic intrinsic
procedure inc(var x; n: Integer = 1); external nil name '::u_system::p_inc'; // generic intrinsic
procedure dec(var x; n: Integer = 1); external nil name '::u_system::p_dec'; // generic intrinsic
// Omitted types express the part Pascal can declare; SetMutation metadata
// checks the missing relationship `values: set of T; item: T`.
procedure include(var values; const item); external nil name '::u_system::p_include'; // generic set intrinsic
procedure exclude(var values; const item); external nil name '::u_system::p_exclude'; // generic set intrinsic
procedure str(const x: Int64; var s: ShortString); overload; external nil name '::u_system::p_str';
procedure str(const x: QWord; var s: ShortString); overload; external nil name '::u_system::p_str';
procedure str(const x: Extended; var s: ShortString); overload; external nil name '::u_system::p_str';
procedure val(const s: ShortString; out value: ShortInt); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: ShortInt; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: SmallInt); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: SmallInt; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: LongInt); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: LongInt; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Int64); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Int64; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Byte); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Byte; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Word); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Word; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: LongWord); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: LongWord; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: QWord); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: QWord; out code); overload; external nil name '::u_system::p_val';
// FIXME: Real is absent because tpcc does not model its target-dependent
// Pascal carrier yet.
procedure val(const s: ShortString; out value: Single); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Single; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Double); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Double; out code); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Extended); overload; external nil name '::u_system::p_val';
procedure val(const s: ShortString; out value: Extended; out code); overload; external nil name '::u_system::p_val';
// FIXME: Comp is absent because tpcc has no Pascal Comp type or carrier.
// FIXME: Currency is absent because tpcc has no Pascal Currency type or
// fixed-scale representation.
// FIXME: Enumeration Val needs generated name-to-ordinal metadata; tpcc
// currently emits enum values but no runtime lookup table for their names.
function octstr(value: LongInt; count: Byte): ShortString; overload; external nil name '::u_system::p_octstr';
function octstr(value: Int64; count: Byte): ShortString; overload; external nil name '::u_system::p_octstr';
function octstr(value: QWord; count: Byte): ShortString; overload; external nil name '::u_system::p_octstr';
function strlen(value: PChar): SizeInt; external nil name '::u_system::p_strlen';
procedure getmem(out destination: Pointer; size: PtrUInt); overload; external nil name '::u_system::p_getmem';
function getmem(size: PtrUInt): Pointer; overload; external nil name '::u_system::p_getmem';
procedure freemem(value: Pointer; size: PtrUInt); overload; external nil name '::u_system::p_freemem';
function freemem(value: Pointer): PtrUInt; overload; external nil name '::u_system::p_freemem';
procedure assign(out f: File; const name: ShortString); external nil name '::u_system::p_assign';
procedure rewrite(var f: File; recordsize: LongInt = 128); external nil name '::u_system::p_rewrite';
procedure reset(var f: File; recordsize: LongInt = 128); external nil name '::u_system::p_reset';
procedure close(var f: File); external nil name '::u_system::p_close';
procedure seek(var f: File; position: Int64); external nil name '::u_system::p_seek';
function filepos(var f: File): Int64; external nil name '::u_system::p_filepos';
function filesize(var f: File): Int64; external nil name '::u_system::p_filesize';
function eof(var f: File): Boolean; external nil name '::u_system::p_eof';
procedure truncate(var f: File); external nil name '::u_system::p_truncate';
function ioresult: Word; external nil name '::u_system::p_ioresult';
procedure blockread(var f: File; var buffer; count: Int64; var result: Int64); external nil name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: LongInt; var result: LongInt); external nil name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Cardinal; var result: Cardinal); external nil name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Word; var result: Word); external nil name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Word; var result: Integer); external nil name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Int64); external nil name '::u_system::p_blockread';
procedure blockwrite(var f: File; const buffer; count: Int64; var result: Int64); external nil name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: LongInt; var result: LongInt); external nil name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Cardinal; var result: Cardinal); external nil name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Word; var result: Word); external nil name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Word; var result: Integer); external nil name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: LongInt); external nil name '::u_system::p_blockwrite';
function assigned(const x: Pointer): Boolean; external nil name '::u_system::p_assigned';
function trunc(const x: Extended): Int64; external nil name '::u_system::p_trunc';
function round(const x: Extended): Int64; external nil name '::u_system::p_round';
function frac(const x: Extended): Extended; external nil name '::u_system::p_frac';
function sqrt(const x: Extended): Extended; external nil name '::u_system::p_sqrt';
function exp(const x: Extended): Extended; external nil name '::u_system::p_exp';
function ln(const x: Extended): Extended; external nil name '::u_system::p_ln';
function pos(const needle: ShortString; const haystack: ShortString): LongInt; overload; external nil name '::u_system::p_pos';
function pos(const needle: ShortString; const haystack: AnsiString): LongInt; overload; external nil name '::u_system::p_pos';
function pos(const needle: AnsiString; const haystack: AnsiString): LongInt; overload; external nil name '::u_system::p_pos';
function pos(needle: Char; const haystack: ShortString): LongInt; overload; external nil name '::u_system::p_pos';
function copy(const value: ShortString; index, count: LongInt): ShortString; overload; external nil name '::u_system::p_copy';
function copy(const value: AnsiString; index, count: LongInt): AnsiString; overload; external nil name '::u_system::p_copy';
function copy(value: Char; index, count: LongInt): ShortString; overload; external nil name '::u_system::p_copy';
procedure delete(var value: ShortString; index, count: LongInt); overload; external nil name '::u_system::p_delete';
procedure delete(var value: AnsiString; index, count: LongInt); overload; external nil name '::u_system::p_delete';
procedure insert(const source: ShortString; var destination: ShortString; index: LongInt); overload; external nil name '::u_system::p_insert';
procedure insert(source: Char; var destination: ShortString; index: LongInt); overload; external nil name '::u_system::p_insert';
procedure insert(const source: AnsiString; var destination: AnsiString; index: LongInt); overload; external nil name '::u_system::p_insert';

operator xor(a, b: Boolean): Boolean; external nil name '::u_system::p_logicalxor';
operator not(a: Boolean): Boolean; external nil name '::u_system::p_logicalnot';

operator =(a, b: shortstring): Boolean; external nil name '::u_system::p_equal';
operator +(a, b: shortstring): shortstring; external nil name '::u_system::p_add';
operator :=(a: shortstring): ansistring; external nil name '::u_system::p_assign';

procedure SetLength(var destination: AnsiString; value: LongInt); external nil name '::u_system::p_setlength';
procedure UniqueString(var value: AnsiString); external nil name '::u_system::p_uniquestring';

implementation

function tpcc_new_instance(meta: TClass): TObject;
  external nil name '::u_system::m_new_instance';

constructor TObject.Create;
begin
end;

destructor TObject.Destroy;
begin
end;

class function TObject.NewInstance: TObject;
begin
  Result := tpcc_new_instance(Self)
end;

procedure TObject.AfterConstruction;
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
