unit sysutils;
interface

type
  Exception = class(TObject)
  private
    FMessage: String;
    FHelpContext: LongInt;
  public
    constructor Create(const Msg: String);
    constructor CreateHelp(const Msg: String; AHelpContext: LongInt);
    property HelpContext: LongInt read FHelpContext write FHelpContext;
    property Message: String read FMessage write FMessage;
  end;
  EIntError = class(Exception);
  ERangeError = class(EIntError);
  EIntOverflow = class(EIntError);
  EHeapMemoryError = class(Exception);
  EOutOfMemory = class(EHeapMemoryError);
  EAccessViolation = class(Exception);
  EAbstractError = class(Exception);
  EInOutError = class(Exception)
  public
    ErrorCode: Integer;
  end;
  EOSError = class(Exception)
  public
    ErrorCode: Integer;
  end;

function Supports(a: TObject; b: TClass): Boolean; external name '::u_system::p_supports';

implementation

procedure RunErrorToException(ErrorCode: LongInt;
  Address, Frame: Pointer);
var
  E: Exception;
begin
  case ErrorCode of
    201: E := ERangeError.Create('Range check error');
    215: E := EIntOverflow.Create('Arithmetic overflow');
    203: E := EOutOfMemory.Create('Out of memory');
    211: E := EAbstractError.Create('Abstract method called');
    216: E := EAccessViolation.Create('Access violation');
    2, 3, 4, 5, 6, 15, 100, 101, 102, 103, 104, 105, 106:
      begin
        E := EInOutError.Create('I/O error');
        EInOutError(E).ErrorCode := ErrorCode
      end
  else
    E := Exception.Create('Runtime error')
  end;
  raise E at Address, Frame
end;

procedure ReportUnhandledException(E: TObject;
  Address, Frame: Pointer);
begin
  if Assigned(E) and (Address = Frame) then
    WriteLn('Unhandled Pascal exception')
  else
    WriteLn('Unhandled Pascal exception')
end;

constructor Exception.Create(const Msg: String);
begin
  FMessage := Msg;
  FHelpContext := 0
end;

constructor Exception.CreateHelp(const Msg: String; AHelpContext: LongInt);
begin
  FMessage := Msg;
  FHelpContext := AHelpContext
end;

initialization
  ErrorProc := @RunErrorToException;
  ExceptProc := @ReportUnhandledException;

finalization
  ExceptProc := nil;
  ErrorProc := nil;

end.
