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
  EConvertError = class(Exception);
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
  ExtensionSeparator = '.';
  fmShareDenyNone = $0040;
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

function Supports(a: TObject; b: TClass): Boolean; external name '::u_sysutils::p_supports';
function CompareText(const S1: AnsiString; const S2: AnsiString): Integer;
function IntToStr(Value: LongInt): AnsiString; overload;
function IntToStr(Value: Int64): AnsiString; overload;
function IntToStr(Value: QWord): AnsiString; overload;
function StrToInt(const S: String): LongInt;
function StrPas(Str: PChar): AnsiString;
function IncludeTrailingPathDelimiter(const Path: AnsiString): AnsiString;
function ExtractFileName(const FileName: AnsiString): AnsiString;
function ExtractFilePath(const FileName: AnsiString): AnsiString;
function ChangeFileExt(const FileName, Extension: AnsiString): AnsiString;
function DeleteFile(const FileName: AnsiString): Boolean; external name '::u_sysutils::p_deletefile';
function FileAge(const FileName: AnsiString): LongInt; external name '::u_sysutils::p_fileage';
function FileExists(const FileName: AnsiString; FollowLink: Boolean = True): Boolean; external name '::u_sysutils::p_fileexists';
function DirectoryExists(const Directory: AnsiString; FollowLink: Boolean = True): Boolean; external name '::u_sysutils::p_directoryexists';
function ExpandFileName(const FileName: AnsiString): AnsiString; external name '::u_sysutils::p_expandfilename';
function GetEnvironmentVariable(const Name: AnsiString): AnsiString; external name '::u_sysutils::p_getenvironmentvariable';
procedure GetLocalTime(var SystemTime: TSystemTime); external name '::u_sysutils::p_getlocaltime';
function FileDateToDateTime(FileDate: LongInt): TDateTime; external name '::u_sysutils::p_filedatetodatetime';
procedure DecodeDate(Date: TDateTime; out Year, Month, Day: Word); external name '::u_sysutils::p_decodedate';
procedure DecodeTime(Time: TDateTime; out Hour, Minute, Second, Millisecond: Word); external name '::u_sysutils::p_decodetime';
function ExecuteProcess(const Path, ComLine: AnsiString;
  Flags: TExecuteFlags = []): Integer; overload;
function ExecuteProcess(const Path: AnsiString;
  const ComLine: array of AnsiString;
  Flags: TExecuteFlags = []): Integer; overload;
function FindFirst(const Path: AnsiString; Attr: LongInt; out Rslt: TSearchRec): LongInt; external name '::u_sysutils::p_findfirst';
function FindNext(var Rslt: TSearchRec): LongInt; external name '::u_sysutils::p_findnext';
procedure FindClose(var F: TSearchRec); external name '::u_sysutils::p_findclose';

function Trim(const S: AnsiString): AnsiString;
function TrimLeft(const S: AnsiString): AnsiString;
function TrimRight(const S: AnsiString): AnsiString;
function StrRScan(p: PChar; c: Char): PChar;
function ExtractFileExt(const FileName: AnsiString): AnsiString;
function SetDirSeparators(const FileName: AnsiString): AnsiString;
function AnsiCompareFileName(const S1, S2: AnsiString): SizeInt;
procedure FreeAndNil(var obj); external name '::u_sysutils::p_freeandnil';

implementation

function StrRScan(p: PChar; c: Char): PChar;
var
  last: PChar;
begin
  last := nil;
  if p <> nil then
    while p^ <> #0 do
      begin
        if p^ = c then
          last := p;
        p := p + 1
      end;
  Result := last
end;

function ExtractFileExt(const FileName: AnsiString): AnsiString;
var
  i: SizeInt;
begin
  Result := '';
  for i := Length(FileName) downto 1 do
    begin
      if FileName[i] = '.' then
        begin
          Result := Copy(FileName, i, Length(FileName) - i + 1);
          Exit
        end
      else if FileName[i] in ['\', '/'] then
        Exit
    end
end;

function SetDirSeparators(const FileName: AnsiString): AnsiString;
var
  i: SizeInt;
begin
  Result := FileName;
  for i := 1 to Length(Result) do
    if Result[i] = '\' then
      Result[i] := '/'
end;

function AnsiCompareFileName(const S1, S2: AnsiString): SizeInt;
var
  i, n: SizeInt;
begin
  n := Length(S1);
  if Length(S2) < n then
    n := Length(S2);
  for i := 1 to n do
    begin
      if S1[i] < S2[i] then
        begin
          Result := -1;
          Exit
        end
      else if S1[i] > S2[i] then
        begin
          Result := 1;
          Exit
        end
    end;
  if Length(S1) < Length(S2) then
    Result := -1
  else if Length(S1) > Length(S2) then
    Result := 1
  else
    Result := 0
end;

function IntToStr(Value: LongInt): AnsiString;
begin
  // System.Str remains the single integer formatting implementation.
  System.Str(Value, Result)
end;

function IntToStr(Value: Int64): AnsiString;
begin
  System.Str(Value, Result)
end;

function IntToStr(Value: QWord): AnsiString;
begin
  System.Str(Value, Result)
end;

function StrToInt(const S: String): LongInt;
var
  Error: Word;
begin
  Val(S, Result, Error);
  if Error <> 0 then
    raise EConvertError.Create('Invalid integer')
end;

function StrPas(Str: PChar): AnsiString;
begin
  { System's PChar-to-AnsiString conversion owns the NUL scan and nil handling. }
  Result := Str
end;

function ExecuteProcessCommandLine(const Path,
  ComLine: AnsiString): Integer;
  external name '::u_sysutils::p_executeprocess_commandline';
function ExecuteProcessArguments(const Path: AnsiString;
  const ComLine: array of AnsiString): Integer;
  external name '::u_sysutils::p_executeprocess_arguments';

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

const
  Whitespace = [#0..' '];

function Trim(const S: AnsiString): AnsiString;
var
  Ofs, Len: LongInt;
begin
  Len := Length(S);
  while (Len > 0) and (S[Len] in Whitespace) do
    Dec(Len);
  Ofs := 1;
  while (Ofs <= Len) and (S[Ofs] in Whitespace) do
    Inc(Ofs);
  Result := Copy(S, Ofs, 1 + Len - Ofs)
end;

function TrimLeft(const S: AnsiString): AnsiString;
var
  Index, Len: LongInt;
begin
  Len := Length(S);
  Index := 1;
  while (Index <= Len) and (S[Index] in Whitespace) do
    Inc(Index);
  Result := Copy(S, Index, Len)
end;

function TrimRight(const S: AnsiString): AnsiString;
var
  Len: LongInt;
begin
  Len := Length(S);
  while (Len > 0) and (S[Len] in Whitespace) do
    Dec(Len);
  Result := Copy(S, 1, Len)
end;

initialization
  ErrorProc := @RunErrorToException;
  ExceptProc := @ReportUnhandledException;

finalization
  ExceptProc := nil;
  ErrorProc := nil;

end.
