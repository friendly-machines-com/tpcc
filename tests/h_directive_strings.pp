program HDirectiveStrings;

type
  { The TPCC/FPC default is H-. }
  TDefaultString = String;

  {$H+}
  TLongString = String;
  TBoundedString = String[7];
  TExplicitShort = ShortString;
  TExplicitAnsi = AnsiString;

  {$push}
  {$H-}
  TPushedShort = String;
  {$pop}
  TRestoredLong = String;

  {$longstrings off}
  TLongStringsOff = String;
  {$longstrings on}
  TLongStringsOn = String;

  {$H-}
  { The directive follows the String token and affects only later tokens. }
  TBeforeFollowingSwitch = String {$H+};
  TAfterFollowingSwitch = String;

var
  DefaultText: TDefaultString;
  LongText: TLongString;
  BoundedText: TBoundedString;
  ExplicitShortText: TExplicitShort;
  ExplicitAnsiText: TExplicitAnsi;
  PushedShortText: TPushedShort;
  RestoredLongText: TRestoredLong;
  LongStringsOffText: TLongStringsOff;
  LongStringsOnText: TLongStringsOn;
  BeforeFollowingSwitchText: TBeforeFollowingSwitch;
  AfterFollowingSwitchText: TAfterFollowingSwitch;

function StringKind(const Value: ShortString): Integer; overload;
begin
  if Value = '' then
    Result := 1
  else
    Result := 1
end;

function StringKind(const Value: AnsiString): Integer; overload;
begin
  if Value = '' then
    Result := 2
  else
    Result := 2
end;

function LengthKind(Value: Byte): Integer; overload;
begin
  if Value = 0 then
    Result := 1
  else
    Result := 1
end;

function LengthKind(Value: SizeInt): Integer; overload;
begin
  if Value = 0 then
    Result := 2
  else
    Result := 2
end;

begin
  {$ifopt H+}
  LongText := 'long';
  {$else}
  MissingIfOptHPlus := 1;
  {$endif}

  DefaultText := 'default';
  BoundedText := 'bounded';
  ExplicitShortText := 'short';
  ExplicitAnsiText := 'ansi';
  PushedShortText := 'pushed';
  RestoredLongText := 'restored';
  LongStringsOffText := 'off';
  LongStringsOnText := 'on';
  BeforeFollowingSwitchText := 'before';
  AfterFollowingSwitchText := 'after';

  if StringKind(DefaultText) <> 1 then Halt(1);
  if StringKind(LongText) <> 2 then Halt(2);
  if StringKind(BoundedText) <> 1 then Halt(3);
  if StringKind(ExplicitShortText) <> 1 then Halt(4);
  if StringKind(ExplicitAnsiText) <> 2 then Halt(5);
  if StringKind(PushedShortText) <> 1 then Halt(6);
  if StringKind(RestoredLongText) <> 2 then Halt(7);
  if StringKind(LongStringsOffText) <> 1 then Halt(8);
  if StringKind(LongStringsOnText) <> 2 then Halt(9);
  if StringKind(BeforeFollowingSwitchText) <> 1 then Halt(10);
  if StringKind(AfterFollowingSwitchText) <> 2 then Halt(11);

  if LengthKind(Length(DefaultText)) <> 1 then Halt(12);
  if LengthKind(Length(LongText)) <> 2 then Halt(13);
  if LengthKind(Length(BoundedText)) <> 1 then Halt(14);
  if LengthKind(Length(ExplicitShortText)) <> 1 then Halt(15);
  if LengthKind(Length(ExplicitAnsiText)) <> 2 then Halt(16)
end.
