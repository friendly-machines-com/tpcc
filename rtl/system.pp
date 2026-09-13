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
  PInt64 = ^Int64;
  Boolean = external name '::u_system::t_boolean';
  Char = external name '::u_system::t_char';
  AnsiChar = Char;
  WideChar = external name '::u_system::t_widechar';
  Single = external name '::u_system::t_single';
  Double = external name '::u_system::t_double';
  Real = type Double;
  TDateTime = type Double;
  Extended = external name '::u_system::t_extended';
  Currency = external name '::u_system::t_currency';
  PCurrency = ^Currency;
  Pointer = external name '::u_system::t_pointer';
  Comp = type Int64;
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
  PLongWord = ^LongWord;
  PCardinal = ^Cardinal;
  TFPUException = (exInvalidOp, exDenormalized, exZeroDivide, exOverflow, exUnderflow, exPrecision);
  TFPUExceptionMask = set of TFPUException;
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
  vtAnsiString = 11;

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
  Output: Text external name '::u_system::p_output';
  ExitProc: CodePointer external name '::u_system::p_exitproc';
  ErrorAddr: CodePointer external name '::u_system::p_erroraddr';
  ExitCode: LongInt external name '::u_system::p_exitcode';
  
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

{ These are the remaining direct predefined integer assignments. They remain
  implicit for Pascal compatibility, but their complete source domains do not
  fit their destinations. Declaring the quality here lets assignment, routine
  calls, and operator actuals share the ordinary matcher without teaching the
  compiler a private table of predefined integer narrowing pairs. }
operator ImplicitNarrowing(a: ShortInt): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: ShortInt): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Word): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Word): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: SmallInt): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: SmallInt): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Cardinal): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Cardinal): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Byte; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): Byte; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): Byte; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: Byte): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Byte): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Word): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Word): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: SmallInt): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: SmallInt): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Cardinal): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Cardinal): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): ShortInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): ShortInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): ShortInt; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: ShortInt): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: ShortInt): Word; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: SmallInt): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: SmallInt): Word; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Cardinal): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Cardinal): Word; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): Word; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Word; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): Word; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): Word; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: Word): SmallInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Word): SmallInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Cardinal): SmallInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Cardinal): SmallInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): SmallInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): SmallInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): SmallInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): SmallInt; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): SmallInt; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): SmallInt; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: ShortInt): Cardinal; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: ShortInt): Cardinal; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: SmallInt): Cardinal; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: SmallInt): Cardinal; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): Cardinal; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): Cardinal; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): Cardinal; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Cardinal; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): Cardinal; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): Cardinal; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: Cardinal): Integer; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Cardinal): Integer; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): Integer; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Integer; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): Integer; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): Integer; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: ShortInt): QWord; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: ShortInt): QWord; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: SmallInt): QWord; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: SmallInt): QWord; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Integer): QWord; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Integer): QWord; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Int64): QWord; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): QWord; external name '::u_system::o_unchecked_implicit';

operator ImplicitNarrowing(a: QWord): Int64; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Int64; external name '::u_system::o_unchecked_implicit';

{ Real narrowing has the same declaration-level quality as integer narrowing.
  Precision loss is inherent; $R distinguishes only finite exponent overflow
  from the defined unchecked infinity result. }
operator ImplicitNarrowing(a: Double): Single; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Double): Single; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Extended): Single; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Extended): Single; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Extended): Double; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Extended): Double; external name '::u_system::o_unchecked_implicit';

// These complete integer domains fit after Currency's factor-of-10000
// scaling, so := declares them as lossless assignment edges. Wider integers
// use the narrowing declaration pair below.
operator :=(a: Byte): Currency; external name '::u_system::o_implicit';
operator :=(a: ShortInt): Currency; external name '::u_system::o_implicit';
operator :=(a: Word): Currency; external name '::u_system::o_implicit';
operator :=(a: SmallInt): Currency; external name '::u_system::o_implicit';
operator :=(a: Cardinal): Currency; external name '::u_system::o_implicit';
operator :=(a: Integer): Currency; external name '::u_system::o_implicit';

{ These domains are not ordered by value-set inclusion. Checked and unchecked
  declarations expose the same conversion edge to overload resolution; $R
  selects only which implementation executes after that resolution. }
operator ImplicitNarrowing(a: Int64): Currency; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Int64): Currency; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: QWord): Currency; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: QWord): Currency; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Single): Currency; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Single): Currency; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Double): Currency; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Double): Currency; external name '::u_system::o_unchecked_implicit';
operator ImplicitNarrowing(a: Extended): Currency; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Extended): Currency; external name '::u_system::o_unchecked_implicit';

{ Every Currency value lies within the real exponent ranges, but a binary
  real cannot preserve Currency's complete decimal grid. Both implementations
  execute the same range-safe operation, but the declared rank remains
  narrowing because information loss and runtime range failure are separate. }
operator ImplicitNarrowing(a: Currency): Single; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Currency): Single; external name '::u_system::o_implicit';
operator ImplicitNarrowing(a: Currency): Double; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Currency): Double; external name '::u_system::o_implicit';
operator ImplicitNarrowing(a: Currency): Extended; external name '::u_system::o_implicit';
operator UncheckedImplicitNarrowing(a: Currency): Extended; external name '::u_system::o_implicit';

operator Explicit(a: Int64): Currency; external name '::u_system::o_explicit';
operator Explicit(a: QWord): Currency; external name '::u_system::o_explicit';
operator Explicit(a: Single): Currency; external name '::u_system::o_explicit';
operator Explicit(a: Double): Currency; external name '::u_system::o_explicit';
operator Explicit(a: Extended): Currency; external name '::u_system::o_explicit';

operator Explicit(a: Currency): Byte; external name '::u_system::o_explicit';
operator Explicit(a: Currency): ShortInt; external name '::u_system::o_explicit';
operator Explicit(a: Currency): Word; external name '::u_system::o_explicit';
operator Explicit(a: Currency): SmallInt; external name '::u_system::o_explicit';
operator Explicit(a: Currency): Integer; external name '::u_system::o_explicit';
operator Explicit(a: Currency): Cardinal; external name '::u_system::o_explicit';
operator Explicit(a: Currency): Int64; external name '::u_system::o_explicit';
operator Explicit(a: Currency): QWord; external name '::u_system::o_explicit';

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
operator UncheckedAdd(a, b: Currency): Currency; external name '::u_system::o_unchecked_add';
operator Add(a, b: Currency): Currency; external name '::u_system::o_add';

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
operator Positive(a: Currency): Currency; external name '::u_system::o_positive';
operator UncheckedNegative(a: Currency): Currency; external name '::u_system::o_unchecked_negative';
operator Negative(a: Currency): Currency; external name '::u_system::o_negative';

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
operator UncheckedSubtract(a, b: Currency): Currency; external name '::u_system::o_unchecked_subtract';
operator Subtract(a, b: Currency): Currency; external name '::u_system::o_subtract';

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
operator UncheckedMultiply(a, b: Currency): Currency; external name '::u_system::o_unchecked_multiply';
operator Multiply(a, b: Currency): Currency; external name '::u_system::o_multiply';

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
// Real division does not rescale back into Currency. The result domain is
// named here, just like the integer `/` rows, rather than manufactured from a
// target-dependent compiler "best real" rule.
operator Divide(a, b: Currency): Double; external name '::u_system::o_divide';

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

operator <(a, b: WideChar): Boolean; external name '::u_system::o_lessthan';
operator <=(a, b: WideChar): Boolean; external name '::u_system::o_lessthanorequal';
operator =(a, b: WideChar): Boolean; external name '::u_system::o_equal';
operator >(a, b: WideChar): Boolean; external name '::u_system::o_greaterthan';
operator >=(a, b: WideChar): Boolean; external name '::u_system::o_greaterthanorequal';

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
operator <(a, b: Currency): Boolean; external name '::u_system::o_lessthan';

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
operator <=(a, b: Currency): Boolean; external name '::u_system::o_lessthanorequal';

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
operator =(a, b: Currency): Boolean; external name '::u_system::o_equal';

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
operator >(a, b: Currency): Boolean; external name '::u_system::o_greaterthan';

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
operator >=(a, b: Currency): Boolean; external name '::u_system::o_greaterthanorequal';

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

// A shift count has one domain independent of the value/result carrier.
// QWord contains every count for which these fixed-width shifts have a
// defined result; larger counts remain outside the language contract.
operator shl(a: Byte; b: QWord): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: ShortInt; b: QWord): Integer; external name '::u_system::o_leftshift';
operator shl(a: Word; b: QWord): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: SmallInt; b: QWord): Integer; external name '::u_system::o_leftshift';
operator shl(a: Cardinal; b: QWord): Cardinal; external name '::u_system::o_leftshift';
operator shl(a: Integer; b: QWord): Integer; external name '::u_system::o_leftshift';
operator shl(a: Int64; b: QWord): Int64; external name '::u_system::o_leftshift';
operator shl(a: QWord; b: QWord): QWord; external name '::u_system::o_leftshift';
operator shr(a: Byte; b: QWord): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: ShortInt; b: QWord): Integer; external name '::u_system::o_rightshift';
operator shr(a: Word; b: QWord): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: SmallInt; b: QWord): Integer; external name '::u_system::o_rightshift';
operator shr(a: Cardinal; b: QWord): Cardinal; external name '::u_system::o_rightshift';
operator shr(a: Integer; b: QWord): Integer; external name '::u_system::o_rightshift';
operator shr(a: Int64; b: QWord): Int64; external name '::u_system::o_rightshift';
operator shr(a: QWord; b: QWord): QWord; external name '::u_system::o_rightshift';
operator in(const item; const values): Boolean; external name '::u_system::o_in';

function ord(const x): Cardinal; external name '::u_system::p_ord'; // generic intrinsic
// FIXME: The selected Chr call must range-check Value under {$R+}. The
// unchecked RTL path intentionally retains FPC's low-byte behavior.
function chr(value: Integer): Char; overload; external name '::u_system::p_chr';
function chr(value: Cardinal): Char; overload; external name '::u_system::p_chr';
function chr(value: Int64): Char; overload; external name '::u_system::p_chr';
function chr(value: QWord): Char; overload; external name '::u_system::p_chr';
{ SwapEndian reverses the bytes of the selected integer carrier. Signed
  overloads preserve the resulting bit pattern; this is not arithmetic
  negation and the const argument is never modified in place. }
function SwapEndian(const value: SmallInt): SmallInt; overload; external name '::u_system::p_swapendian';
function SwapEndian(const value: Word): Word; overload; external name '::u_system::p_swapendian';
function SwapEndian(const value: LongInt): LongInt; overload; external name '::u_system::p_swapendian';
function SwapEndian(const value: DWord): DWord; overload; external name '::u_system::p_swapendian';
function SwapEndian(const value: Int64): Int64; overload; external name '::u_system::p_swapendian';
function SwapEndian(const value: QWord): QWord; overload; external name '::u_system::p_swapendian';
{ Lo and Hi select the lower and higher of two parts of an integer slot. }
function Lo(const value: Byte): Byte; overload;
function Lo(const value: ShortInt): Byte; overload;
function Lo(const value: Word): Byte; overload;
function Lo(const value: SmallInt): Byte; overload;
function Lo(const value: LongInt): Word; overload;
function Lo(const value: DWord): Word; overload;
function Lo(const value: Int64): DWord; overload;
function Lo(const value: QWord): DWord; overload;
function Hi(const value: Byte): Byte; overload;
function Hi(const value: ShortInt): Byte; overload;
function Hi(const value: Word): Byte; overload;
function Hi(const value: SmallInt): Byte; overload;
function Hi(const value: LongInt): Word; overload;
function Hi(const value: DWord): Word; overload;
function Hi(const value: Int64): DWord; overload;
function Hi(const value: QWord): DWord; overload;
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
{ IndexByte's length is measured in bytes; IndexWord's is measured in native
  Word elements. Both return a zero-based element index, or -1 when absent. }
function indexbyte(const buf; len: SizeInt; value: Byte): SizeInt; external name '::u_system::p_indexbyte';
function indexword(const buf; len: SizeInt; value: Word): SizeInt; external name '::u_system::p_indexword';
function comparebyte(const buf1, buf2; len: SizeInt): SizeInt; external name '::u_system::p_comparebyte';
function comparechar(const buf1, buf2; len: SizeInt): SizeInt; external name '::u_system::p_comparechar';
{ CompareWord compares len native Word elements lexicographically. }
function compareword(const buf1, buf2; len: SizeInt): SizeInt; external name '::u_system::p_compareword';
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
// Str is the textual projection used by Write/WriteLn. Textual inputs project
// their existing character sequence; numeric and enumeration inputs produce
// their declared textual representation. The omitted destination preserves
// each String[N] type's exact capacity rather than introducing var-parameter
// covariance. The omitted source remains only for compiler-owned families
// which Pascal cannot quantify over directly, such as integer subranges and
// named enumerations.
// Each complete small-integer domain has an exact declaration. Otherwise an
// Integer actual would face incomparable lossless conversions to the Int64
// and Currency projections; the library must state its intended textual
// domains instead of asking ranking to prefer one numeric family.
procedure str(const x: Byte; var s); overload; external name '::u_system::p_str';
procedure str(const x: ShortInt; var s); overload; external name '::u_system::p_str';
procedure str(const x: Word; var s); overload; external name '::u_system::p_str';
procedure str(const x: SmallInt; var s); overload; external name '::u_system::p_str';
procedure str(const x: Integer; var s); overload; external name '::u_system::p_str';
procedure str(const x: Cardinal; var s); overload; external name '::u_system::p_str';
procedure str(const x: Int64; var s); overload; external name '::u_system::p_str';
procedure str(const x: QWord; var s); overload; external name '::u_system::p_str';
procedure str(const x: Extended; var s); overload; external name '::u_system::p_str';
procedure str(const x: Currency; var s); overload; external name '::u_system::p_str';
procedure str(const x: Char; var s); overload; external name '::u_system::p_str';
procedure str(const x: ShortString; var s); overload; external name '::u_system::p_str';
procedure str(const x: AnsiString; var s); overload; external name '::u_system::p_str';
procedure str(const x: PChar; var s); overload; external name '::u_system::p_str';
procedure str(const x; var s); overload; external name '::u_system::p_str';
// AnsiString needs concrete overloads because `var` parameters do not perform
// ShortString-to-AnsiString assignment conversion. These select the unbounded
// managed-string sink while the omitted forms above preserve each String[N]
// destination's exact capacity.
procedure str(const x: Byte; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: ShortInt; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Word; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: SmallInt; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Integer; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Cardinal; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Int64; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: QWord; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Extended; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Currency; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: Char; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: ShortString; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: AnsiString; var s: AnsiString); overload; external name '::u_system::p_str';
procedure str(const x: PChar; var s: AnsiString); overload; external name '::u_system::p_str';
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
// TODO: Comp is absent.
procedure val(const s: ShortString; out value: Currency); overload; external name '::u_system::p_val';
procedure val(const s: ShortString; out value: Currency; out code); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Currency); overload; external name '::u_system::p_val';
procedure val(const s: AnsiString; out value: Currency; out code); overload; external name '::u_system::p_val';
// FIXME: Enumeration Val: use generated name-to-ordinal metadata.
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
procedure rewrite(var f: File; recordsize: LongInt = 128); overload; external name '::u_system::p_rewrite';
procedure rewrite(var f: Text); overload; external name '::u_system::p_rewrite';
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
// Old-style Pascal I/O has one pending status. With I/O checking disabled, the
// first failure sets that status and later I/O operations are skipped.
// IOResult returns and clears it; the operation after IOResult must therefore
// execute independently of the failure which produced the returned status.
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

{ SetString copies a counted character range. Buf is not required to point to
  a null-terminated string, and embedded #0 characters are ordinary data. A
  nil Buf still sets the requested length without reading source storage. }
procedure SetString(out S: AnsiString; Buf: PAnsiChar; Len: SizeInt); overload; external name '::u_system::p_setstring';
{ The omitted destination preserves each String[N] type's capacity. The
  SetString builtin contract admits only ShortString destinations here; this
  is not general out-parameter covariance. }
procedure SetString(out S; Buf: PAnsiChar; Len: SizeInt); overload; external name '::u_system::p_setstring';
procedure SetLength(var destination: AnsiString; value: SizeInt); overload; external name '::u_system::p_setlength';
{ The omitted type covers dynamic arrays and every fixed-capacity ShortString
  type without weakening ordinary exact-type rules for var parameters. }
procedure SetLength(var destination; value: SizeInt); overload; external name '::u_system::p_setlength';
procedure UniqueString(var value: AnsiString); external name '::u_system::p_uniquestring';

operator Explicit(const Value: Extended): Comp;

function UpCase(c: Char): Char;
function UpCase(const s: shortstring): shortstring;
function Odd(l: ShortInt): Boolean;
function Odd(l: Byte): Boolean;
function Odd(l: SmallInt): Boolean;
function Odd(l: Word): Boolean;
function Odd(l: LongInt): Boolean;
function Odd(l: LongWord): Boolean;
function Odd(l: Int64): Boolean;
function Odd(l: QWord): Boolean;
function StrPas(p: PChar): shortstring;
function StringOfChar(c: AnsiChar; l: SizeInt): AnsiString;
function Pi: Double; external name '::u_system::p_pi';
function Sin(d: Double): Double; external name '::u_system::p_sin';
function Cos(d: Double): Double; external name '::u_system::p_cos';
function ArcTan(d: Double): Double; external name '::u_system::p_arctan';
function Int(d: Double): Double; external name '::u_system::p_int';
function Frac(d: Double): Double; external name '::u_system::p_frac';
function IsNan(const d: Single): Boolean; overload; external name '::u_system::p_isnan_single';
function IsNan(const d: Double): Boolean; overload; external name '::u_system::p_isnan_double';
function IsNan(const d: Extended): Boolean; overload; external name '::u_system::p_isnan_extended';
function IsInfinite(const d: Single): Boolean; overload; external name '::u_system::p_isinf_single';
function IsInfinite(const d: Double): Boolean; overload; external name '::u_system::p_isinf_double';
function IsInfinite(const d: Extended): Boolean; overload; external name '::u_system::p_isinf_extended';
function RolByte(Const AValue: Byte): Byte;
function RolByte(Const AValue: Byte; const Dist: Byte): Byte;
function RorByte(Const AValue: Byte): Byte;
function RorByte(Const AValue: Byte; const Dist: Byte): Byte;
function RolWord(Const AValue: Word): Word;
function RolWord(Const AValue: Word; const Dist: Byte): Word;
function RorWord(Const AValue: Word): Word;
function RorWord(Const AValue: Word; const Dist: Byte): Word;
function RolDWord(Const AValue: DWord): DWord;
function RolDWord(Const AValue: DWord; const Dist: Byte): DWord;
function RorDWord(Const AValue: DWord): DWord;
function RorDWord(Const AValue: DWord; const Dist: Byte): DWord;
function RolQWord(Const AValue: QWord): QWord;
function RolQWord(Const AValue: QWord; const Dist: Byte): QWord;
function RorQWord(Const AValue: QWord): QWord;
function RorQWord(Const AValue: QWord; const Dist: Byte): QWord;
function BsfByte(Const AValue: Byte): Byte;
function BsrByte(Const AValue: Byte): Byte;
function BsfWord(Const AValue: Word): Cardinal;
function BsrWord(Const AValue: Word): Cardinal;
function BsfDWord(Const AValue: DWord): Cardinal;
function BsrDWord(Const AValue: DWord): Cardinal;
function BsfQWord(Const AValue: QWord): Cardinal;
function BsrQWord(Const AValue: QWord): Cardinal;
function PopCnt(Const AValue: Byte): Byte;
function PopCnt(Const AValue: Word): Word;
function PopCnt(Const AValue: DWord): DWord;
function PopCnt(Const AValue: QWord): QWord;

implementation

function UpCase(c: Char): Char;
begin
  if c in ['a'..'z'] then
    Result := Char(Ord(c) - 32)
  else
    Result := c
end;

function UpCase(const s: shortstring): shortstring;
var
  i: SizeInt;
begin
  Result := s;
  for i := 1 to Length(s) do
    Result[i] := UpCase(s[i])
end;

function Odd(l: ShortInt): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: Byte): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: SmallInt): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: Word): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: LongInt): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: LongWord): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: Int64): Boolean; begin Result := (l and 1) <> 0 end;
function Odd(l: QWord): Boolean; begin Result := (l and 1) <> 0 end;

function StrPas(p: PChar): shortstring;
var
  len, i: SizeInt;
begin
  len := 0;
  if p <> nil then
    while (len < 255) and (p[len] <> #0) do
      Inc(len);
  SetLength(Result, len);
  for i := 0 to len - 1 do
    Result[i + 1] := p[i]
end;

function StringOfChar(c: AnsiChar; l: SizeInt): AnsiString;
var
  i: SizeInt;
begin
  SetLength(Result, l);
  for i := 1 to l do
    Result[i] := c
end;

function RolByte(Const AValue: Byte): Byte;
begin
  Result := Byte((AValue shl 1) or (AValue shr 7))
end;

function RolByte(Const AValue: Byte; const Dist: Byte): Byte;
var
  d: Byte;
begin
  d := Dist and 7;
  Result := Byte((AValue shl d) or (AValue shr Byte((8 - d) and 7)))
end;

function RorByte(Const AValue: Byte): Byte;
begin
  Result := Byte((AValue shr 1) or (AValue shl 7))
end;

function RorByte(Const AValue: Byte; const Dist: Byte): Byte;
var
  d: Byte;
begin
  d := Dist and 7;
  Result := Byte((AValue shr d) or (AValue shl Byte((8 - d) and 7)))
end;

function RolWord(Const AValue: Word): Word;
begin
  Result := Word((AValue shl 1) or (AValue shr 15))
end;

function RolWord(Const AValue: Word; const Dist: Byte): Word;
var
  d: Byte;
begin
  d := Dist and 15;
  Result := Word((AValue shl d) or (AValue shr Byte((16 - d) and 15)))
end;

function RorWord(Const AValue: Word): Word;
begin
  Result := Word((AValue shr 1) or (AValue shl 15))
end;

function RorWord(Const AValue: Word; const Dist: Byte): Word;
var
  d: Byte;
begin
  d := Dist and 15;
  Result := Word((AValue shr d) or (AValue shl Byte((16 - d) and 15)))
end;

function RolDWord(Const AValue: DWord): DWord;
begin
  Result := DWord((AValue shl 1) or (AValue shr 31))
end;

function RolDWord(Const AValue: DWord; const Dist: Byte): DWord;
var
  d: Byte;
begin
  d := Dist and 31;
  Result := DWord((AValue shl d) or (AValue shr Byte((32 - d) and 31)))
end;

function RorDWord(Const AValue: DWord): DWord;
begin
  Result := DWord((AValue shr 1) or (AValue shl 31))
end;

function RorDWord(Const AValue: DWord; const Dist: Byte): DWord;
var
  d: Byte;
begin
  d := Dist and 31;
  Result := DWord((AValue shr d) or (AValue shl Byte((32 - d) and 31)))
end;

function RolQWord(Const AValue: QWord): QWord;
begin
  Result := QWord((AValue shl 1) or (AValue shr 63))
end;

function RolQWord(Const AValue: QWord; const Dist: Byte): QWord;
var
  d: Byte;
begin
  d := Dist and 63;
  Result := QWord((AValue shl d) or (AValue shr Byte((64 - d) and 63)))
end;

function RorQWord(Const AValue: QWord): QWord;
begin
  Result := QWord((AValue shr 1) or (AValue shl 63))
end;

function RorQWord(Const AValue: QWord; const Dist: Byte): QWord;
var
  d: Byte;
begin
  d := Dist and 63;
  Result := QWord((AValue shr d) or (AValue shl Byte((64 - d) and 63)))
end;

function BsfByte(Const AValue: Byte): Byte;
var
  i: Byte;
begin
  Result := $ff;
  for i := 0 to 7 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsrByte(Const AValue: Byte): Byte;
var
  i: Byte;
begin
  Result := $ff;
  for i := 7 downto 0 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsfWord(Const AValue: Word): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 0 to 15 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsrWord(Const AValue: Word): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 15 downto 0 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsfDWord(Const AValue: DWord): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 0 to 31 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsrDWord(Const AValue: DWord): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 31 downto 0 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsfQWord(Const AValue: QWord): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 0 to 63 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function BsrQWord(Const AValue: QWord): Cardinal;
var
  i: Cardinal;
begin
  Result := $ffffffff;
  for i := 63 downto 0 do
    if ((AValue shr i) and 1) <> 0 then
      begin
        Result := i;
        Break
      end
end;

function PopCnt(Const AValue: Byte): Byte;
var
  i: Byte;
begin
  Result := 0;
  for i := 0 to 7 do
    Inc(Result, Byte((AValue shr i) and 1))
end;

function PopCnt(Const AValue: Word): Word;
var
  i: Cardinal;
begin
  Result := 0;
  for i := 0 to 15 do
    Inc(Result, Word((AValue shr i) and 1))
end;

function PopCnt(Const AValue: DWord): DWord;
var
  i: Cardinal;
begin
  Result := 0;
  for i := 0 to 31 do
    Inc(Result, DWord((AValue shr i) and 1))
end;

function PopCnt(Const AValue: QWord): QWord;
var
  i: Cardinal;
begin
  Result := 0;
  for i := 0 to 63 do
    Result := Result + QWord((AValue shr i) and 1)
end;

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
  Result := 'TObject'
end;

{ ClassType() of a metaclass is the metaclass itself, so a root body that
  delegates through ClassType() redispatches this same virtual operation
  forever. The root answers directly; descendants' generated defaults chain
  into these bodies only after their own receiver check fails. }
class function TObject.InheritsFrom(klass: TClass): Boolean;
begin
  Result := klass = TObject
end;

class function TObject.ClassParent: TClass;
begin
  Result := nil
end;

operator Explicit(const Value: Extended): Comp;
begin
  Result := Round(Value)
end;

function Lo(const value: Byte): Byte;
begin
  Result := Byte(value and $0F)
end;

function Lo(const value: ShortInt): Byte;
begin
  Result := Byte(value)
end;

function Lo(const value: Word): Byte;
begin
  Result := Byte(value)
end;

function Lo(const value: SmallInt): Byte;
begin
  Result := Byte(value)
end;

function Lo(const value: LongInt): Word;
begin
  Result := Word(value)
end;

function Lo(const value: DWord): Word;
begin
  Result := Word(value)
end;

function Lo(const value: Int64): DWord;
begin
  Result := DWord(value)
end;

function Lo(const value: QWord): DWord;
begin
  Result := DWord(value)
end;

function Hi(const value: Byte): Byte;
begin
  Result := Byte(value shr 4)
end;

function Hi(const value: ShortInt): Byte;
begin
  Result := Byte(value shr 8)
end;

function Hi(const value: Word): Byte;
begin
  Result := Byte(value shr 8)
end;

function Hi(const value: SmallInt): Byte;
begin
  Result := Byte(value shr 8)
end;

function Hi(const value: LongInt): Word;
begin
  Result := Word(value shr 16)
end;

function Hi(const value: DWord): Word;
begin
  Result := Word(value shr 16)
end;

function Hi(const value: Int64): DWord;
begin
  Result := DWord(value shr 32)
end;

function Hi(const value: QWord): DWord;
begin
  Result := DWord(value shr 32)
end;

end.
