program IfOpt;

{$Q+}
var
{$ifopt Q+}
  QPlus: Integer;
{$else}
  WrongQPlus: MissingType;
{$endif}

{$ifopt Q-}
  WrongQMinus: MissingType;
{$else}
  QMinusRejected: Integer;
{$endif}

{$ifdef NEVER}
  {$Q-}
  {$ifopt Q+}
    WrongNestedInner: MissingType;
  {$endif}
  WrongNestedOuter: MissingType;
{$endif}

{$ifopt Q+}
  OuterStatePreserved: Integer;
{$else}
  WrongOuterState: MissingType;
{$endif}

{$Q-}
{$ifopt Q-}
  QMinus: Integer;
{$else}
  WrongAfterQMinus: MissingType;
{$endif}

{$ifopt R-}
  RInitiallyMinus: Integer;
{$else}
  WrongInitialR: MissingType;
{$endif}

{$R+}
{$ifopt Q-}
  {$ifopt R+}
    NestedActive: Integer;
  {$else}
    WrongNestedR: MissingType;
  {$endif}
{$endif}

function InlineEnabled(A: Integer): Integer;
begin
  InlineEnabled := {$ifopt R+} A + {$endif} 1
end;

{$R-}
function InlineDisabled(A: Integer): Integer;
begin
  InlineDisabled := {$ifopt R+} A + {$endif} 1
end;

begin
  QPlus := 1;
  QMinusRejected := 2;
  OuterStatePreserved := 3;
  QMinus := 4;
  RInitiallyMinus := 5;
  NestedActive := 6;
  if InlineEnabled(1) <> 2 then
    QPlus := 0;
  if InlineDisabled(1) <> 1 then
    QPlus := 0
end.
