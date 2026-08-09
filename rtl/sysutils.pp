unit sysutils;
interface

type
  TExecuteFlag = (ExecInheritsHandles);
  TExecuteFlags = Set of TExecuteFlag;
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
  TSearchRec = record
    Time: LongInt;
    Size: Int64;
    Attr: LongInt;
    Name: AnsiString;
    ExcludeAttr: LongInt;
    FindHandle: Pointer;
    Mode: LongInt
  end;
  TRawByteSearchRec = TSearchRec;
  TSystemTime = record
    Year, Month, DayOfWeek, Day, Hour, Minute, Second,  Millisecond: Word
  end;
  TProcedure = procedure;

const
  faReadOnly = $00000001;
  faHidden = $00000002;
  faSysFile = $00000004;
  faVolumeId = $00000008;
  faDirectory = $00000010;
  faArchive = $00000020;
  faNormal = $00000080;
  faTemporary = $00000100;
  faSymLink = $00000400;
  faCompressed = $00000800;
  faEncrypted = $00004000;
  faVirtual = $00010000;
  faAnyFile = $000001FF;

function Supports(a: TObject; b: TClass): Boolean; external name '::u_system::p_supports';
function CompareText(const S1: AnsiString; const S2: AnsiString): Integer;
function IncludeTrailingPathDelimiter(const Path: AnsiString): AnsiString;
function ExtractFileName(const FileName: AnsiString): AnsiString;
function ExtractFilePath(const FileName: AnsiString): AnsiString;
function ChangeFileExt(const FileName, Extension: AnsiString): AnsiString;
function FileExists(const FileName: AnsiString; FollowLink: Boolean = True): Boolean; external name '::u_system::p_fileexists';
function DirectoryExists(const Directory: AnsiString; FollowLink: Boolean = True): Boolean; external name '::u_system::p_directoryexists';
function ExpandFileName(const FileName: AnsiString): AnsiString; external name '::u_system::p_expandfilename';
function GetEnvironmentVariable(const Name: AnsiString): AnsiString; external name '::u_system::p_getenvironmentvariable';
procedure GetLocalTime(var SystemTime: TSystemTime); external name '::u_system::p_getlocaltime';
function ExecuteProcess(const Path, ComLine: AnsiString;
  Flags: TExecuteFlags = []): Integer; overload;
function ExecuteProcess(const Path: AnsiString;
  const ComLine: array of AnsiString;
  Flags: TExecuteFlags = []): Integer; overload;
function FindFirst(const Path: AnsiString; Attr: LongInt; out Rslt: TSearchRec): LongInt; external name '::u_system::p_findfirst';
function FindNext(var Rslt: TSearchRec): LongInt; external name '::u_system::p_findnext';
procedure FindClose(var F: TSearchRec); external name '::u_system::p_findclose';

implementation

function ExecuteProcessCommandLine(const Path,
  ComLine: AnsiString): Integer;
  external name '::u_system::p_executeprocess_commandline';
function ExecuteProcessArguments(const Path: AnsiString;
  const ComLine: array of AnsiString): Integer;
  external name '::u_system::p_executeprocess_arguments';

procedure RaiseExecuteProcessError(Status: Integer);
var
  E: EOSError;
begin
  E := EOSError.Create('Failed to execute process');
  E.ErrorCode := Status;
  raise E
end;

function ExecuteProcess(const Path, ComLine: AnsiString;
  Flags: TExecuteFlags = []): Integer;
begin
  Result := ExecuteProcessCommandLine(Path, ComLine);
  if (Result < 0) or (Result = 127) then
    RaiseExecuteProcessError(Result)
end;

function ExecuteProcess(const Path: AnsiString;
  const ComLine: array of AnsiString;
  Flags: TExecuteFlags = []): Integer;
begin
  Result := ExecuteProcessArguments(Path, ComLine);
  if (Result < 0) or (Result = 127) then
    RaiseExecuteProcessError(Result)
end;

function LastPathDelimiter(
  const FileName: AnsiString): SizeInt;
begin
  Result := Length(FileName);
  while Result > 0 do
    begin
      if FileName[Result] in AllowDirectorySeparators then
        Break;
      Result := Result - 1
    end
end;

function ExtractFileName(
  const FileName: AnsiString): AnsiString;
var
  Delimiter: SizeInt;
begin
  Delimiter := LastPathDelimiter(FileName);
  Result := Copy(FileName, Delimiter + 1,
    Length(FileName) - Delimiter)
end;

function ExtractFilePath(
  const FileName: AnsiString): AnsiString;
var
  Delimiter: SizeInt;
begin
  Delimiter := LastPathDelimiter(FileName);
  Result := Copy(FileName, 1, Delimiter)
end;

function ChangeFileExt(const FileName, Extension: AnsiString): AnsiString;
var
  Index: SizeInt;
  StartsFileName: Boolean;
begin
  Index := Length(FileName);
  while (Index > 0) and not ((FileName[Index] = '.') or (FileName[Index] in AllowDirectorySeparators)) do
    Index := Index - 1;

  if (Index = 0) or (FileName[Index] <> '.') then
    Index := Length(FileName) + 1
  else
    begin
      StartsFileName := (Index = 1) or (FileName[Index - 1] in AllowDirectorySeparators);
      if StartsFileName then
        Index := Length(FileName) + 1
    end;

  Result := Copy(FileName, 1, Index - 1) + Extension
end;

function IncludeTrailingPathDelimiter(
  const Path: AnsiString): AnsiString;
var
  Count: SizeInt;
begin
  Result := Path;
  Count := Length(Result);
  if Count = 0 then
    Result := '/'
  else if not (Result[Count] in AllowDirectorySeparators) then
    Result := Result + '/'
end;

function CompareText(const S1: AnsiString; const S2: AnsiString): Integer;
var
  I, Count, Count1, Count2: SizeInt;
  Chr1, Chr2: Byte;
  P1, P2: PChar;
begin
  Count1 := Length(S1);
  Count2 := Length(S2);
  if Count1 > Count2 then
    Count := Count2
  else
    Count := Count1;
  I := 0;
  if Count > 0 then
    begin
      P1 := @S1[1];
      P2 := @S2[1];
      while I < Count do
        begin
          Chr1 := Byte(P1^);
          Chr2 := Byte(P2^);
          if Chr1 <> Chr2 then
            begin
              if Chr1 in [97..122] then
                Dec(Chr1, 32);
              if Chr2 in [97..122] then
                Dec(Chr2, 32);
              if Chr1 <> Chr2 then
                Break
            end;
          Inc(P1);
          Inc(P2);
          Inc(I)
        end
    end;
  if I < Count then
    Result := Chr1 - Chr2
  else
    Result := Count1 - Count2
end;

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
    2, 3, 4, 5, 6, 12, 15, 16, 100, 101, 102, 103, 104, 105, 106, 156:
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
