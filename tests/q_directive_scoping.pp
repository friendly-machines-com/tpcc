program QDirectiveScoping;

uses SysUtils;

type
  TTag = record
    Selected: Integer;
  end;

operator Add(Left, Right: TTag): TTag;
begin
  Result.Selected := Left.Selected + Right.Selected + 1
end;

operator UncheckedAdd(Left, Right: TTag): TTag;
begin
  Result.Selected := Left.Selected + Right.Selected + 2
end;

var
  I: Integer;
  Left, Selected: TTag;
  Caught: Boolean;
  LoopUnchecked: Integer;
  LoopChecked: Integer;
  DownUnchecked: Integer;
  DownChecked: Integer;

begin
  { A directive in the right subtree cannot retroactively change the outer
    binary operator. }
  {$push}
  {$Q-}
  I := High(Integer);
  Caught := False;
  try
    I := I + {$Q+} 1
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if Caught or (I <> Low(Integer)) then Halt(1);

  { The inverse nesting proves that the inner subtree remains independently
    controllable while the outer operation keeps its earlier policy. }
  {$push}
  {$Q+}
  I := High(Integer);
  Caught := False;
  try
    I := I + ({$Q-} 0 + 1)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(2);

  {$push}
  {$Q-}
  I := Low(Integer);
  I := -({$Q+} I);
  {$pop}
  if I <> Low(Integer) then Halt(3);

  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    I := -({$Q-} I)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(4);

  {$push}
  {$Q-}
  I := Low(Integer);
  I := I - {$Q+} 1;
  {$pop}
  if I <> High(Integer) then Halt(17);
  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    I := I - {$Q-} 1
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(18);

  {$push}
  {$Q-}
  I := High(Integer);
  I := I * {$Q+} 2;
  {$pop}
  if I <> -2 then Halt(19);
  {$push}
  {$Q+}
  I := High(Integer);
  Caught := False;
  try
    I := I * {$Q-} 2
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(20);

  {$push}
  {$Q-}
  I := Low(Integer);
  I := I div {$Q+} -1;
  {$pop}
  if I <> Low(Integer) then Halt(21);
  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    I := I div {$Q-} -1
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(22);

  { A custom family is selected at the + token by the same saved policy. }
  Left.Selected := 0;
  {$push}
  {$Q-}
  Selected := Left + ({$Q+} Left);
  {$pop}
  if Selected.Selected <> 2 then Halt(5);
  {$push}
  {$Q+}
  Selected := Left + ({$Q-} Left);
  {$pop}
  if Selected.Selected <> 1 then Halt(6);

  { Direct compiler intrinsics anchor at their identifier, including when a
    directive appears between that identifier and its opening parenthesis. }
  {$push}
  {$Q-}
  I := Low(Integer);
  I := Abs {$Q+} (I);
  {$pop}
  if I <> Low(Integer) then Halt(7);
  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    I := Abs({$Q-} I)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(8);

  {$push}
  {$Q-}
  I := High(Integer);
  I := Succ({$Q+} I);
  {$pop}
  if I <> Low(Integer) then Halt(9);
  {$push}
  {$Q+}
  I := High(Integer);
  Caught := False;
  try
    I := Succ({$Q-} I)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(10);

  {$push}
  {$Q-}
  I := Low(Integer);
  I := Pred({$Q+} I);
  {$pop}
  if I <> High(Integer) then Halt(11);
  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    I := Pred({$Q-} I)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(12);

  { Unary and distance mutations both anchor at Inc/Dec, before their
    destination and amount subtrees. }
  {$push}
  {$Q-}
  I := High(Integer);
  Inc {$Q+} (I);
  {$pop}
  if I <> Low(Integer) then Halt(13);
  {$push}
  {$Q+}
  I := High(Integer);
  Caught := False;
  try
    Inc({$Q-} I)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(14);

  {$push}
  {$Q-}
  I := High(Integer);
  Inc(I, {$Q+} 1);
  {$pop}
  if I <> Low(Integer) then Halt(15);
  {$push}
  {$Q+}
  I := Low(Integer);
  Caught := False;
  try
    Dec(I, {$Q-} 1)
  except
    on EIntOverflow do Caught := True
  end;
  {$pop}
  if not Caught then Halt(16);

  { Loop stepping is not observable at an invalid carrier value because the
    terminal guard suppresses the artificial final step. Generated-code
    assertions in the test script verify the saved for-token policy. }
  {$push}
  {$Q-}
  for {$Q+} LoopUnchecked := 1 to 2 do
    I := I;
  {$pop}
  {$push}
  {$Q+}
  for {$Q-} LoopChecked := 1 to 2 do
    I := I;
  {$pop}
  {$push}
  {$Q-}
  for {$Q+} DownUnchecked := 2 downto 1 do
    I := I;
  {$pop}
  {$push}
  {$Q+}
  for {$Q-} DownChecked := 2 downto 1 do
    I := I;
  {$pop}
end.
