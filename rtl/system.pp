unit system;

interface

type 
  Byte = external name '::u_system::t_byte';
  ShortInt = external name '::u_system::t_shortint';
  Word = external name '::u_system::t_word';
  SmallInt = external name '::u_system::t_smallint';
  LongWord = external name '::u_system::t_longword';
  DWord = LongWord;
  Cardinal = LongWord;
  Integer = external name '::u_system::t_integer';
  LongInt = Integer;
  QWord = external name '::u_system::t_qword';
  Int64 = external name '::u_system::t_int64';
  Boolean = external name '::u_system::t_boolean';
  Char = external name '::u_system::t_char';
  AnsiChar = Char;
  Single = external name '::u_system::t_single';
  Double = external name '::u_system::t_double';
  Real = type Double;
  TDateTime = type Double;
  Extended = external name '::u_system::t_extended';
  Pointer = external name '::u_system::t_pointer';
  // TPCC supports only flat 32/64-bit targets, where code and data addresses
  // share the same pointer representation. The distinct Pascal name remains
  // useful in the public stack-inspection signatures.
  CodePointer = Pointer;
  PPointer = ^Pointer;
  TMethod = external name '::u_system::t_tmethod';
  // FIXME: A 32-bit -P target must alias PtrInt/SizeInt to LongInt and
  // PtrUInt/SizeUInt to LongWord. The current System model is explicitly the
  // FPC 64-bit variant, where these names have canonical Int64/QWord identity.
  PtrInt = Int64;
  PtrUInt = QWord;
  SizeInt = Int64;
  SizeUInt = QWord;
  // The predefined ShortString is the concrete type String[255]. It does not
  // mean "String[N] for any N" and is not an open-string formal; each
  // explicitly bounded String[N] retains its own compile-time capacity.
  shortstring = external name '::u_system::t_shortstring<255>';
  Text = external name '::u_system::t_text';
  TextFile = Text;
  PShortString = ^shortstring;
  PChar = ^Char;
  PAnsiChar = PChar;
  AnsiString = external name '::u_system::t_ansistring';
  // `class of X` is a real class-reference type in the compiler. Its C++
  // carrier is a pointer to the empty target-specific base implemented by
  // X's metaclass, so X may still be incomplete at the declaration site.
  TClass = class of TObject;
  TErrorProc = procedure(ErrorCode: LongInt; Address, Frame: Pointer);
  TObject = class
  public
    constructor Create;
    destructor Destroy; virtual;
    procedure Free; external name '::u_system::m_free_object';
    
    // Class methods live on the generated m_meta class. A class name supplies
    // its exact metaclass; an object supplies its dynamic metaclass through
    // the compiler-generated virtual object-to-class-reference conversion.
    class function ClassType: TClass; virtual; external name 'p_classtype';
    class function ClassName: shortstring; virtual;
    class function InheritsFrom(klass: TClass): Boolean; virtual;
    class function ClassParent: TClass; virtual;
    class function NewInstance: TObject; virtual;
    procedure FreeInstance; virtual; external name 'p_freeinstance';
    class function InstanceSize: SizeInt; virtual; external name 'p_instancesize';
    procedure AfterConstruction; virtual;
  end;
  TExceptProc = procedure(ExceptObject: TObject;
    Address, Frame: Pointer);
  TSysCharSet = set of Char;
  SignalHandler = procedure; // FIXME: cdecl;

const
  MaxLongint = $7fffffff;
  MaxSmallint = $7fff;
  MaxInt = MaxSmallint;
  AllowDirectorySeparators: TSysCharSet = ['\', '/'];
  DirectorySeparator: Char = '/';
  DriveSeparator = '';
  PathSeparator: Char = ':';

var
  StdOut: Text external name '::u_system::p_stdout';
  StdErr: Text external name '::u_system::p_stderr';
  // RunError stores its error number in this RTL variable before terminating.
  ErrorCode: Word external name '::u_system::p_errorcode';
  // System reports language runtime failures without depending on SysUtils.
  // SysUtils installs its ordinary Pascal routine here to translate those
  // numeric errors into Pascal exception objects; without it RunError retains
  // System's terminating behavior.
  ErrorProc: TErrorProc external name '::u_system::p_errorproc';
  // The generated program entry calls this after finalizing initialized
  // Pascal units when a Pascal exception reaches the outer boundary.
  ExceptProc: TExceptProc;
  // Reset(File) consults the low two access-mode bits. Higher sharing-mode
  // bits are retained for source compatibility and ignored by this runtime.
  FileMode: Byte external name '::u_system::p_filemode';
  
operator Positive(a: Cardinal): Cardinal; external name '::u_system::o_positive';
operator Positive(a: Integer): Integer; external name '::u_system::o_positive';
operator Positive(a: QWord): QWord; external name '::u_system::o_positive';
operator Positive(a: Int64): Int64; external name '::u_system::o_positive';
operator Positive(a: Single): Single; external name '::u_system::o_positive';
operator Positive(a: Double): Double; external name '::u_system::o_positive';
operator Positive(a: Extended): Extended; external name '::u_system::o_positive';

// These are the complete direct, lossless integer assignment edges between
// the fixed-width predefined types. Transitive pairs are intentionally
// present: implicit conversion may use one operator := edge, never a chain.
operator :=(a: ShortInt): SmallInt; external name '::u_system::o_implicit';
operator :=(a: ShortInt): Integer; external name '::u_system::o_implicit';
operator :=(a: ShortInt): Int64; external name '::u_system::o_implicit';

operator :=(a: Byte): SmallInt; external name '::u_system::o_implicit';
operator :=(a: Byte): Word; external name '::u_system::o_implicit';
operator :=(a: Byte): Integer; external name '::u_system::o_implicit';
operator :=(a: Byte): Cardinal; external name '::u_system::o_implicit';
operator :=(a: Byte): Int64; external name '::u_system::o_implicit';
operator :=(a: Byte): QWord; external name '::u_system::o_implicit';

operator :=(a: SmallInt): Integer; external name '::u_system::o_implicit';
operator :=(a: SmallInt): Int64; external name '::u_system::o_implicit';

operator :=(a: Word): Integer; external name '::u_system::o_implicit';
operator :=(a: Word): Cardinal; external name '::u_system::o_implicit';
operator :=(a: Word): Int64; external name '::u_system::o_implicit';
operator :=(a: Word): QWord; external name '::u_system::o_implicit';

operator :=(a: Integer): Int64; external name '::u_system::o_implicit';

operator :=(a: Cardinal): Int64; external name '::u_system::o_implicit';
operator :=(a: Cardinal): QWord; external name '::u_system::o_implicit';

// The parser chooses one of these ordinary operator families before overload
// resolution. Keeping both rows explicit also lets user-defined arithmetic
// make the same checked/unchecked promise as System arithmetic.
// Each homogeneous real row contributes its formal type as a common-domain
// proposal.
operator UncheckedAdd(a, b: Byte): Integer; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: ShortInt): Integer; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Word): Integer; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: SmallInt): Integer; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Cardinal): Cardinal; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Integer): Integer; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: QWord): QWord; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Int64): Int64; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Single): Single; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Double): Double; external name '::u_system::o_unchecked_add';
operator UncheckedAdd(a, b: Extended): Extended; external name '::u_system::o_unchecked_add';
operator Add(a, b: Byte): Integer; external name '::u_system::o_add';
operator Add(a, b: ShortInt): Integer; external name '::u_system::o_add';
operator Add(a, b: Word): Integer; external name '::u_system::o_add';
operator Add(a, b: SmallInt): Integer; external name '::u_system::o_add';
operator Add(a, b: Cardinal): Cardinal; external name '::u_system::o_add';
operator Add(a, b: Integer): Integer; external name '::u_system::o_add';
operator Add(a, b: QWord): QWord; external name '::u_system::o_add';
operator Add(a, b: Int64): Int64; external name '::u_system::o_add';
operator Add(a, b: Single): Single; external name '::u_system::o_add';
operator Add(a, b: Double): Double; external name '::u_system::o_add';
operator Add(a, b: Extended): Extended; external name '::u_system::o_add';

operator UncheckedNegative(a: Cardinal): Cardinal; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: Integer): Integer; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: QWord): QWord; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: Int64): Int64; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: Single): Single; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: Double): Double; external name '::u_system::o_unchecked_negative';
operator UncheckedNegative(a: Extended): Extended; external name '::u_system::o_unchecked_negative';
operator Negative(a: Cardinal): Cardinal; external name '::u_system::o_negative';
operator Negative(a: Integer): Integer; external name '::u_system::o_negative';
operator Negative(a: QWord): QWord; external name '::u_system::o_negative';
operator Negative(a: Int64): Int64; external name '::u_system::o_negative';
operator Negative(a: Single): Single; external name '::u_system::o_negative';
operator Negative(a: Double): Double; external name '::u_system::o_negative';
operator Negative(a: Extended): Extended; external name '::u_system::o_negative';

operator UncheckedSubtract(a, b: Byte): Integer; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: ShortInt): Integer; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Word): Integer; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: SmallInt): Integer; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Cardinal): Cardinal; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Integer): Integer; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: QWord): QWord; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Int64): Int64; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Single): Single; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Double): Double; external name '::u_system::o_unchecked_subtract';
operator UncheckedSubtract(a, b: Extended): Extended; external name '::u_system::o_unchecked_subtract';
operator Subtract(a, b: Byte): Integer; external name '::u_system::o_subtract';
operator Subtract(a, b: ShortInt): Integer; external name '::u_system::o_subtract';
operator Subtract(a, b: Word): Integer; external name '::u_system::o_subtract';
operator Subtract(a, b: SmallInt): Integer; external name '::u_system::o_subtract';
operator Subtract(a, b: Cardinal): Cardinal; external name '::u_system::o_subtract';
operator Subtract(a, b: Integer): Integer; external name '::u_system::o_subtract';
operator Subtract(a, b: QWord): QWord; external name '::u_system::o_subtract';
operator Subtract(a, b: Int64): Int64; external name '::u_system::o_subtract';
operator Subtract(a, b: Single): Single; external name '::u_system::o_subtract';
operator Subtract(a, b: Double): Double; external name '::u_system::o_subtract';
operator Subtract(a, b: Extended): Extended; external name '::u_system::o_subtract';

operator UncheckedMultiply(a, b: Byte): Integer; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: ShortInt): Integer; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Word): Integer; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: SmallInt): Integer; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Cardinal): Cardinal; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Integer): Integer; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: QWord): QWord; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Int64): Int64; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Single): Single; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Double): Double; external name '::u_system::o_unchecked_multiply';
operator UncheckedMultiply(a, b: Extended): Extended; external name '::u_system::o_unchecked_multiply';
operator Multiply(a, b: Byte): Integer; external name '::u_system::o_multiply';
operator Multiply(a, b: ShortInt): Integer; external name '::u_system::o_multiply';
operator Multiply(a, b: Word): Integer; external name '::u_system::o_multiply';
operator Multiply(a, b: SmallInt): Integer; external name '::u_system::o_multiply';
operator Multiply(a, b: Cardinal): Cardinal; external name '::u_system::o_multiply';
operator Multiply(a, b: Integer): Integer; external name '::u_system::o_multiply';
operator Multiply(a, b: QWord): QWord; external name '::u_system::o_multiply';
operator Multiply(a, b: Int64): Int64; external name '::u_system::o_multiply';
operator Multiply(a, b: Single): Single; external name '::u_system::o_multiply';
operator Multiply(a, b: Double): Double; external name '::u_system::o_multiply';
operator Multiply(a, b: Extended): Extended; external name '::u_system::o_multiply';

// FIXME: FPC uses Double, Delphi uses Extended
operator Divide(a, b: Byte): Double; external name '::u_system::o_divide';
operator Divide(a, b: ShortInt): Double; external name '::u_system::o_divide';
operator Divide(a, b: Word): Double; external name '::u_system::o_divide';
operator Divide(a, b: SmallInt): Double; external name '::u_system::o_divide';
operator Divide(a, b: Cardinal): Double; external name '::u_system::o_divide';
operator Divide(a, b: Integer): Double; external name '::u_system::o_divide';
operator Divide(a, b: QWord): Double; external name '::u_system::o_divide';
operator Divide(a, b: Int64): Double; external name '::u_system::o_divide';
operator Divide(a, b: Single): Single; external name '::u_system::o_divide';
operator Divide(a, b: Double): Double; external name '::u_system::o_divide';
operator Divide(a, b: Extended): Extended; external name '::u_system::o_divide';

{ Exponentiation is asymmetric: the base selects the result domain, while the
  exponent has its own destination. It therefore does not use the homogeneous
  arithmetic fallback. }
operator **(Base, Exponent: Integer): Integer; external name '::u_system::o_power';
operator **(Base: Cardinal; Exponent: Integer): Cardinal; external name '::u_system::o_power';
operator **(Base: Int64; Exponent: Integer): Int64; external name '::u_system::o_power';
operator **(Base: QWord; Exponent: Integer): QWord; external name '::u_system::o_power';
operator **(Base, Exponent: Extended): Extended; external name '::u_system::o_power';

operator :=(a: Char): ShortString; external name '::u_system::o_implicit';

operator <(a, b: Char): Boolean; external name '::u_system::o_lessthan';
operator <=(a, b: Char): Boolean; external name '::u_system::o_lessthanorequal';
operator =(a, b: Char): Boolean; external name '::u_system::o_equal';
operator >(a, b: Char): Boolean; external name '::u_system::o_greaterthan';
operator >=(a, b: Char): Boolean; external name '::u_system::o_greaterthanorequal';

// Compare PChar by address, not by the characters they point to.
operator <(a, b: PChar): Boolean; external name '::u_system::o_lessthan';
operator <=(a, b: PChar): Boolean; external name '::u_system::o_lessthanorequal';
operator =(a, b: PChar): Boolean; external name '::u_system::o_equal';
operator >(a, b: PChar): Boolean; external name '::u_system::o_greaterthan';
operator >=(a, b: PChar): Boolean; external name '::u_system::o_greaterthanorequal';

// Typed and untyped pointers share Pascal's ordinary pointer equality. Call
// matching tests each source operand against this Pointer formal, including
// contextual nil, then applies the selected formal carrier after selection.
// No operand is pre-cast merely to choose an operator overload.
operator =(a, b: Pointer): Boolean; external name '::u_system::o_equal';

// Class instances and metaclasses may be retained as opaque untyped Pointer
// values by a deliberately low-priority predefined conversion. This TObject
// overload still wins for related class operands, preserving class identity
// comparison; a more specific custom Equal overload wins by normal ranking.
operator =(a, b: TObject): Boolean; external name '::u_system::o_equal';

// Relational rows mirror the arithmetic domains so both families use the same
// common-domain relation.
operator <(a, b: Byte): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: ShortInt): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Word): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: SmallInt): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Cardinal): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Integer): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: QWord): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Int64): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Single): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Double): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: Extended): Boolean; external name '::u_system::o_lessthan';

operator <=(a, b: Byte): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: ShortInt): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Word): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: SmallInt): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Cardinal): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Integer): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: QWord): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Int64): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Single): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Double): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: Extended): Boolean; external name '::u_system::o_lessthanorequal';

operator =(a, b: Byte): Boolean; external name '::u_system::o_equal';
operator =(a, b: ShortInt): Boolean; external name '::u_system::o_equal';
operator =(a, b: Word): Boolean; external name '::u_system::o_equal';
operator =(a, b: SmallInt): Boolean; external name '::u_system::o_equal';
operator =(a, b: Cardinal): Boolean; external name '::u_system::o_equal';
operator =(a, b: Integer): Boolean; external name '::u_system::o_equal';
operator =(a, b: QWord): Boolean; external name '::u_system::o_equal';
operator =(a, b: Int64): Boolean; external name '::u_system::o_equal';
operator =(a, b: Single): Boolean; external name '::u_system::o_equal';
operator =(a, b: Double): Boolean; external name '::u_system::o_equal';
operator =(a, b: Extended): Boolean; external name '::u_system::o_equal';

operator >(a, b: Byte): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: ShortInt): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Word): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: SmallInt): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Cardinal): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Integer): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: QWord): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Int64): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Single): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Double): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: Extended): Boolean; external name '::u_system::o_greaterthan';

operator >=(a, b: Byte): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: ShortInt): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Word): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: SmallInt): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Cardinal): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Integer): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: QWord): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Int64): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Single): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Double): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: Extended): Boolean; external name '::u_system::o_greaterthanorequal';

operator UncheckedIntDivide(a, b: Byte): Integer; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: ShortInt): Integer; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: Word): Integer; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: SmallInt): Integer; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: Cardinal): Cardinal; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: Integer): Integer; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: QWord): QWord; external name '::u_system::o_unchecked_intdivide';
operator UncheckedIntDivide(a, b: Int64): Int64; external name '::u_system::o_unchecked_intdivide';
operator IntDivide(a, b: Byte): Integer; external name '::u_system::o_intdivide';
operator IntDivide(a, b: ShortInt): Integer; external name '::u_system::o_intdivide';
operator IntDivide(a, b: Word): Integer; external name '::u_system::o_intdivide';
operator IntDivide(a, b: SmallInt): Integer; external name '::u_system::o_intdivide';
operator IntDivide(a, b: Cardinal): Cardinal; external name '::u_system::o_intdivide';
operator IntDivide(a, b: Integer): Integer; external name '::u_system::o_intdivide';
operator IntDivide(a, b: QWord): QWord; external name '::u_system::o_intdivide';
operator IntDivide(a, b: Int64): Int64; external name '::u_system::o_intdivide';
operator Modulus(a, b: Byte): Integer; external name '::u_system::o_modulus';
operator Modulus(a, b: ShortInt): Integer; external name '::u_system::o_modulus';
operator Modulus(a, b: Word): Integer; external name '::u_system::o_modulus';
operator Modulus(a, b: SmallInt): Integer; external name '::u_system::o_modulus';
operator Modulus(a, b: Cardinal): Cardinal; external name '::u_system::o_modulus';
operator Modulus(a, b: Integer): Integer; external name '::u_system::o_modulus';
operator Modulus(a, b: QWord): QWord; external name '::u_system::o_modulus';
operator Modulus(a, b: Int64): Int64; external name '::u_system::o_modulus';

// Delphi exposes only the named unary operator LogicalNot: the same `not`
// token means Boolean negation for Boolean and width-preserving bitwise
// complement for integers. Keep every concrete integer signature here so
// overload resolution preserves the operand type instead of widening it.
operator not(a: Byte): Byte; external name '::u_system::o_logicalnot';
operator not(a: ShortInt): ShortInt; external name '::u_system::o_logicalnot';
operator not(a: Word): Word; external name '::u_system::o_logicalnot';
operator not(a: SmallInt): SmallInt; external name '::u_system::o_logicalnot';
operator not(a: Cardinal): Cardinal; external name '::u_system::o_logicalnot';
operator not(a: Integer): Integer; external name '::u_system::o_logicalnot';
operator not(a: QWord): QWord; external name '::u_system::o_logicalnot';
operator not(a: Int64): Int64; external name '::u_system::o_logicalnot';
operator and(a, b: Byte): Integer; external name '::u_system::o_bitwiseand';
operator and(a, b: ShortInt): Integer; external name '::u_system::o_bitwiseand';
operator and(a, b: Word): Integer; external name '::u_system::o_bitwiseand';
operator and(a, b: SmallInt): Integer; external name '::u_system::o_bitwiseand';
operator and(a, b: Cardinal): Cardinal; external name '::u_system::o_bitwiseand';
operator and(a, b: Integer): Integer; external name '::u_system::o_bitwiseand';
operator and(a, b: QWord): QWord; external name '::u_system::o_bitwiseand';
operator and(a, b: Int64): Int64; external name '::u_system::o_bitwiseand';
operator or(a, b: Byte): Integer; external name '::u_system::o_bitwiseor';
operator or(a, b: ShortInt): Integer; external name '::u_system::o_bitwiseor';
operator or(a, b: Word): Integer; external name '::u_system::o_bitwiseor';
operator or(a, b: SmallInt): Integer; external name '::u_system::o_bitwiseor';
operator or(a, b: Cardinal): Cardinal; external name '::u_system::o_bitwiseor';
operator or(a, b: Integer): Integer; external name '::u_system::o_bitwiseor';
operator or(a, b: QWord): QWord; external name '::u_system::o_bitwiseor';
operator or(a, b: Int64): Int64; external name '::u_system::o_bitwiseor';
operator xor(a, b: Byte): Integer; external name '::u_system::o_bitwisexor';
operator xor(a, b: ShortInt): Integer; external name '::u_system::o_bitwisexor';
operator xor(a, b: Word): Integer; external name '::u_system::o_bitwisexor';
operator xor(a, b: SmallInt): Integer; external name '::u_system::o_bitwisexor';
operator xor(a, b: Cardinal): Cardinal; external name '::u_system::o_bitwisexor';
operator xor(a, b: Integer): Integer; external name '::u_system::o_bitwisexor';
operator xor(a, b: QWord): QWord; external name '::u_system::o_bitwisexor';
operator xor(a, b: Int64): Int64; external name '::u_system::o_bitwisexor';

operator shl(a: Byte; b: Integer): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: ShortInt; b: Integer): Integer; external name '::u_system::o_leftshift';
operator shl(a: Word; b: Integer): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: SmallInt; b: Integer): Integer; external name '::u_system::o_leftshift';
operator shl(a: Cardinal; b: Integer): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: Integer; b: Integer): Integer; external name '::u_system::o_leftshift';
operator shl(a: Int64; b: Integer): Int64; external name '::u_system::o_leftshift';
operator shl(a: QWord; b: Integer): QWord; external name '::u_system::o_leftshift';
operator shr(a: Byte; b: Integer): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: ShortInt; b: Integer): Integer; external name '::u_system::o_rightshift';
operator shr(a: Word; b: Integer): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: SmallInt; b: Integer): Integer; external name '::u_system::o_rightshift';
operator shr(a: Cardinal; b: Integer): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: Integer; b: Integer): Integer; external name '::u_system::o_rightshift';
operator shr(a: Int64; b: Integer): Int64; external name '::u_system::o_rightshift';
operator shr(a: QWord; b: Integer): QWord; external name '::u_system::o_rightshift';
operator in(const item; const values): Boolean; external name '::u_system::o_in';

function ord(const x): Cardinal; external name '::u_system::p_ord'; // generic intrinsic
// FIXME: The selected Chr call must range-check Value under {$R+}. The
// unchecked RTL path intentionally retains FPC's low-byte behavior.
function chr(value: Integer): Char; overload; external name '::u_system::p_chr';
function chr(value: Cardinal): Char; overload; external name '::u_system::p_chr';
function chr(value: Int64): Char; overload; external name '::u_system::p_chr';
function chr(value: QWord): Char; overload; external name '::u_system::p_chr';
procedure fillchar(var destination; count: SizeInt; value: Byte); external name '::u_system::p_fillchar';
procedure fillchar(var destination; count: SizeInt; value: Char); external name '::u_system::p_fillchar';
procedure fillbyte(var destination; count: SizeInt; value: Byte); external name '::u_system::p_fillbyte';
// FillDWord's count is a number of DWord elements, not a byte count.
procedure filldword(var destination; count: SizeInt; value: DWord); external name '::u_system::p_filldword';
procedure prefetch(const memory); external name '::u_system::p_prefetch';
procedure move(const source; var destination; count: SizeInt); external name '::u_system::p_move';
// Initialize/Finalize deliberately expose only FPC's one-value forms. The
// omitted var type retains the caller's exact managed carrier for the RTL
// lifecycle operation; the count overload is not declared.
procedure initialize(var value); external name '::u_system::p_initialize';
procedure finalize(var value); external name '::u_system::p_finalize';
function comparebyte(const buf1, buf2; len: SizeInt): SizeInt; external name '::u_system::p_comparebyte';
function comparechar(const buf1, buf2; len: SizeInt): SizeInt; external name '::u_system::p_comparechar';
function sizeof(const x): SizeInt; external name '::u_system::p_sizeof';
// Write/WriteLn have compiler grammar for a variable number of values and
// `value:width:precision`; these parameterless declarations provide normal
// name lookup and shadowing while BuiltinSyntaxKind parses the actual call.
procedure write; external name '::u_system::p_write';
procedure writeln; external name '::u_system::p_writeln';
procedure flush(var output: Text); external name '::u_system::p_flush';
procedure halt(value: LongInt); overload; noreturn; external name '::u_system::p_halt';
procedure halt; overload; noreturn; external name '::u_system::p_halt';
procedure runerror(value: Word); overload; noreturn; external name '::u_system::p_runerror';
procedure runerror; overload; noreturn; external name '::u_system::p_runerror';
// Linux obtains element zero from /proc/self/exe; the remaining elements are
// the original process arguments. The ShortString result matches FPC's
// System unit, which is compiled under {$H-}.
function paramstr(index: LongInt): ShortString; external name '::u_system::p_paramstr';
function paramcount: LongInt; external name '::u_system::p_paramcount';
// These `m_` external names are intentionally unqualified internal macros, not
// ordinary addressable `p_` functions. A C++ function would observe its own
// frame instead of the generated Pascal call site, while a namespace qualifier
// would remain in front of the preprocessor expansion and make the builtin
// expression invalid.
function get_frame: Pointer; external name 'm_get_frame';
function get_caller_addr(framebp: Pointer; address: CodePointer = nil): CodePointer; external name 'm_get_caller_addr';
function get_caller_frame(framebp: Pointer; address: CodePointer = nil): Pointer; external name 'm_get_caller_frame';
function low(const x): Integer; external name '::u_system::p_low'; // generic intrinsic: parser supplies the type operand/result
function high(const x): Integer; external name '::u_system::p_high'; // generic intrinsic: parser supplies the type operand/result
// ShortString stores its length in one byte. AnsiString has a spellable exact
// SizeInt overload so assignment-legal AnsiString -> ShortString narrowing
// cannot intercept it; the omitted-type fallback covers array families.
function length(const x: ShortString): Byte; overload; external name '::u_system::p_length';
function length(const x: AnsiString): SizeInt; overload; external name '::u_system::p_length';
function length(const x): SizeInt; overload; external name '::u_system::p_length'; // generic intrinsic
// Omitted types express the part Pascal can declare; SetMutation metadata
// checks the missing relationship `values: set of T; item: T`.
procedure include(var values; const item); external name '::u_system::p_include'; // generic set intrinsic
procedure exclude(var values; const item); external name '::u_system::p_exclude'; // generic set intrinsic
// The omitted storage types are intentional compiler contracts, not Pascal
// var/out covariance. They keep string[N] and the selected integer/subrange
// destination intact until the Str/Val semantic handlers validate and lower
// the call. AnsiString has exact Val source declarations because it is a
// distinct managed carrier rather than a member of the string[N] family.
// Concrete source/destination families still participate in normal overload
// ranking; the all-generic declarations are last-resort extension points for
// compiler-owned families such as enumerations.
procedure str(const x: Int64; var s); overload; external name '::u_system::p_str';
procedure str(const x: QWord; var s); overload; external name '::u_system::p_str';
procedure str(const x: Extended; var s); overload; external name '::u_system::p_str';
procedure str(const x; var s); overload; external name '::u_system::p_str';
// AnsiString needs concrete overloads because `var` parameters do not perform
// ShortString-to-AnsiString assignment conversion. These select the unbounded
// managed-string sink while the omitted forms above preserve each String[N]
// destination's exact capacity.
procedure str(const x: Int64; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: QWord; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Extended; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x; var s: AnsiString); overload; external name '::u_system::p_str';
procedure val(const s: ShortString; out value); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value; out code); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value; out code); overload; external name '::u_system::p_val';
// FIXME: Real is absent because tpcc does not model its target-dependent
// Pascal carrier yet.
procedure val(const s: ShortString; out value: Single); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Single; out code); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Single); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Single; out code); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Double); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Double; out code); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Double); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Double; out code); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Extended); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Extended; out code); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Extended); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Extended; out code); overload; external name '::u_system::p_val';
// FIXME: Comp is absent because tpcc has no Pascal Comp type or carrier.
// FIXME: Currency is absent because tpcc has no Pascal Currency type or
// fixed-scale representation.
// FIXME: Enumeration Val needs generated name-to-ordinal metadata; tpcc
// currently emits enum values but no runtime lookup table for their names.
function hexstr(value: LongInt; count: Byte): ShortString; overload; external name '::u_system::p_hexstr';
function hexstr(value: Int64; count: Byte): ShortString; overload; external name '::u_system::p_hexstr';
function hexstr(value: QWord; count: Byte): ShortString; overload; external name '::u_system::p_hexstr';
function hexstr(value: Pointer): ShortString; overload; external name '::u_system::p_hexstr';
function octstr(value: LongInt; count: Byte): ShortString; overload; external name '::u_system::p_octstr';
function octstr(value: Int64; count: Byte): ShortString; overload; external name '::u_system::p_octstr';
function octstr(value: QWord; count: Byte): ShortString; overload; external name '::u_system::p_octstr';
function strlen(value: PChar): SizeInt; external name '::u_system::p_strlen';
// New and Dispose have compiler grammar because their first operand may be a
// type and their optional second operand names an old-object lifecycle method.
// These declarations provide ordinary lookup and shadowing only.
procedure New; external name '::u_system::p_new';
procedure Dispose; external name '::u_system::p_dispose';
procedure getmem(out destination: Pointer; size: PtrUInt); overload; external name '::u_system::p_getmem';
procedure getmem(out destination: Pointer; size: PtrInt); overload; external name '::u_system::p_getmem';
function getmem(size: PtrUInt): Pointer; overload; external name '::u_system::p_getmem';
function getmem(size: PtrInt): Pointer; overload; external name '::u_system::p_getmem';
function allocmem(size: PtrUInt): Pointer; overload; external name '::u_system::p_allocmem';
function allocmem(size: PtrInt): Pointer; overload; external name '::u_system::p_allocmem';
function reallocmem(var destination: Pointer; size: PtrUInt): Pointer; overload; external name '::u_system::p_reallocmem';
function reallocmem(var destination: Pointer; size: PtrInt): Pointer; overload; external name '::u_system::p_reallocmem';
procedure freemem(value: Pointer; size: PtrUInt); overload; external name '::u_system::p_freemem';
procedure freemem(value: Pointer; size: PtrInt); overload; external name '::u_system::p_freemem';
function freemem(value: Pointer): PtrUInt; overload; external name '::u_system::p_freemem';
procedure GetDir(drivenr: Byte; var dir: ShortString); overload; external name '::u_system::p_getdir';
procedure GetDir(drivenr: Byte; var dir: AnsiString); overload; external name '::u_system::p_getdir';
procedure RmDir(const path: ShortString); overload; external name '::u_system::p_rmdir';
procedure RmDir(const path: AnsiString); overload; external name '::u_system::p_rmdir';
procedure assign(out f: File; const name: ShortString); overload; external name '::u_system::p_assign';
procedure assign(out f: Text; const name: ShortString); overload; external name '::u_system::p_assign';
procedure assign(out f: Text; const name: AnsiString); overload; external name '::u_system::p_assign';
procedure rewrite(var f: File; recordsize: LongInt = 128); external name '::u_system::p_rewrite';
procedure reset(var f: File; recordsize: LongInt = 128); overload; external name '::u_system::p_reset';
procedure reset(var f: Text); overload; external name '::u_system::p_reset';
procedure close(var f: File); overload; external name '::u_system::p_close';
procedure close(var f: Text); overload; external name '::u_system::p_close';
procedure seek(var f: File; position: Int64); external name '::u_system::p_seek';
function filepos(var f: File): Int64; external name '::u_system::p_filepos';
function filesize(var f: File): Int64; external name '::u_system::p_filesize';
function eof(var f: File): Boolean; overload; external name '::u_system::p_eof';
function eof(var f: Text): Boolean; overload; external name '::u_system::p_eof';
procedure truncate(var f: File); external name '::u_system::p_truncate';
// SetTextBuf changes only buffering and therefore may be a no-op without
// changing Pascal-visible results. Keep the complete three-argument contract
// so the caller still supplies and type-checks its buffer storage.
procedure settextbuf(var f: Text; var buffer; size: SizeInt); external name '::u_system::p_settextbuf';
procedure readln(var f: Text; out value: ShortString); overload; external name '::u_system::p_readln';
procedure readln(var f: Text; out value: AnsiString); overload; external name '::u_system::p_readln';
function ioresult: Word; external name '::u_system::p_ioresult';
procedure blockread(var f: File; var buffer; count: Int64; var result: Int64); external name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: LongInt; var result: LongInt); external name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Cardinal; var result: Cardinal); external name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Word; var result: Word); external name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Word; var result: Integer); external name '::u_system::p_blockread';
procedure blockread(var f: File; var buffer; count: Int64); external name '::u_system::p_blockread';
procedure blockwrite(var f: File; const buffer; count: Int64; var result: Int64); external name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: LongInt; var result: LongInt); external name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Cardinal; var result: Cardinal); external name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Word; var result: Word); external name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: Word; var result: Integer); external name '::u_system::p_blockwrite';
procedure blockwrite(var f: File; const buffer; count: LongInt); external name '::u_system::p_blockwrite';
function assigned(const x: Pointer): Boolean; external name '::u_system::p_assigned';
function Trunc(const x: Extended): Int64; external name '::u_system::p_trunc';
function Round(const x: Extended): Int64; external name '::u_system::p_round';
function frac(const x: Extended): Extended; external name '::u_system::p_frac';
function sqr(x: Integer): Integer; overload; external name '::u_system::p_sqr';
function sqr(x: Int64): Int64; overload; external name '::u_system::p_sqr';
function sqr(x: QWord): QWord; overload; external name '::u_system::p_sqr';
function sqr(x: Extended): Extended; overload; external name '::u_system::p_sqr';
function sqrt(const x: Extended): Extended; external name '::u_system::p_sqrt';
function exp(const x: Extended): Extended; external name '::u_system::p_exp';
function ln(const x: Extended): Extended; external name '::u_system::p_ln';
function pos(const needle: ShortString; const haystack: ShortString): LongInt; overload; external name '::u_system::p_pos';
function pos(const needle: ShortString; const haystack: AnsiString): LongInt; overload; external name '::u_system::p_pos';
function pos(const needle: AnsiString; const haystack: AnsiString): LongInt; overload; external name '::u_system::p_pos';
function pos(needle: Char; const haystack: ShortString): LongInt; overload; external name '::u_system::p_pos';
function copy(const value: ShortString; index, count: SizeInt): ShortString; overload; external name '::u_system::p_copy';
function copy(const value: AnsiString; index, count: SizeInt): AnsiString; overload; external name '::u_system::p_copy';
function copy(value: Char; index, count: SizeInt): ShortString; overload; external name '::u_system::p_copy';
{ The omitted mutable types preserve the actual String[N] capacity. The
  ShortStringMutation builtin contract rejects every non-ShortString actual;
  this is not general var-parameter covariance or an open-string declaration. }
procedure delete(var value; index, count: SizeInt); overload; external name '::u_system::p_delete';
procedure delete(var value: AnsiString; index, count: SizeInt); overload; external name '::u_system::p_delete';
procedure insert(const source: ShortString; var destination; index: SizeInt); overload; external name '::u_system::p_insert';
procedure insert(source: Char; var destination; index: SizeInt); overload; external name '::u_system::p_insert';
procedure insert(const source: AnsiString; var destination: AnsiString; index: SizeInt); overload; external name '::u_system::p_insert';

operator xor(a, b: Boolean): Boolean; external name '::u_system::o_logicalxor';
operator not(a: Boolean): Boolean; external name '::u_system::o_logicalnot';

operator =(a, b: shortstring): Boolean; external name '::u_system::o_equal';
operator =(a, b: ansistring): Boolean; external name '::u_system::o_equal';
operator <(a, b: shortstring): Boolean; external name '::u_system::o_lessthan';
operator <(a, b: ansistring): Boolean; external name '::u_system::o_lessthan';
operator <=(a, b: shortstring): Boolean; external name '::u_system::o_lessthanorequal';
operator <=(a, b: ansistring): Boolean; external name '::u_system::o_lessthanorequal';
operator >(a, b: shortstring): Boolean; external name '::u_system::o_greaterthan';
operator >(a, b: ansistring): Boolean; external name '::u_system::o_greaterthan';
operator >=(a, b: shortstring): Boolean; external name '::u_system::o_greaterthanorequal';
operator >=(a, b: ansistring): Boolean; external name '::u_system::o_greaterthanorequal';
operator UncheckedAdd(a, b: shortstring): shortstring; external name '::u_system::o_unchecked_add';
operator Add(a, b: shortstring): shortstring; external name '::u_system::o_add';
operator UncheckedAdd(a, b: ansistring): ansistring; external name '::u_system::o_unchecked_add';
operator Add(a, b: ansistring): ansistring; external name '::u_system::o_add';
operator :=(a: shortstring): ansistring; external name '::u_system::o_implicit';
operator :=(a: AnsiChar): AnsiString; external name '::u_system::o_implicit';
operator :=(a: PChar): AnsiString; external name '::u_system::o_implicit';

procedure SetLength(var destination: AnsiString; value: SizeInt); overload; external name '::u_system::p_setlength';
{ The omitted type covers dynamic arrays and every fixed-capacity ShortString
  type without weakening ordinary exact-type rules for var parameters. }
procedure SetLength(var destination; value: SizeInt); overload; external name '::u_system::p_setlength';
procedure UniqueString(var value: AnsiString); external name '::u_system::p_uniquestring';

implementation

function tpcc_new_instance(meta: TClass): TObject;
  external name '::u_system::m_new_instance';

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
