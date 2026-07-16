program DirectivePushPop;

{$Q+}
{$R-}
{$I+}
{$D-}
{$O-}
{$A+}
{$Z+}
{$define RemovedInside}

{$push}
{$R+, Q-}
{$D+}
{$O+}
{$A-}
{$Z-}
{$undef RemovedInside}
{$define AddedInside}

var
{$ifopt Q-}
  InnerQ: Integer;
{$else}
  WrongInnerQ: MissingType;
{$endif}
{$ifopt R+}
  InnerR: Integer;
{$else}
  WrongInnerR: MissingType;
{$endif}
{$ifopt D+}
  InnerD: Integer;
{$else}
  WrongInnerD: MissingType;
{$endif}
{$ifopt O+}
  InnerO: Integer;
{$else}
  WrongInnerO: MissingType;
{$endif}
{$ifopt A-}
  InnerA: Integer;
{$else}
  WrongInnerA: MissingType;
{$endif}
{$ifopt Z-}
  InnerZ: Integer;
{$else}
  WrongInnerZ: MissingType;
{$endif}

{$push}
{$Q+}
{$pop}
{$ifopt Q-}
  NestedLocalRestored: Integer;
{$else}
  WrongNestedLocal: MissingType;
{$endif}

{$include directive_push_pop_set.inc}
{$ifopt I-}
  IncludeChangedI: Integer;
{$else}
  WrongIncludeState: MissingType;
{$endif}
{$pop}

{$ifopt Q+}
  RestoredQ: Integer;
{$else}
  WrongRestoredQ: MissingType;
{$endif}
{$ifopt R-}
  RestoredR: Integer;
{$else}
  WrongRestoredR: MissingType;
{$endif}
{$ifopt I+}
  RestoredI: Integer;
{$else}
  WrongRestoredI: MissingType;
{$endif}
{$ifopt A+}
  RestoredA: Integer;
{$else}
  WrongRestoredA: MissingType;
{$endif}
{$ifopt Z+}
  RestoredZ: Integer;
{$else}
  WrongRestoredZ: MissingType;
{$endif}
{$ifopt D+}
  PersistentD: Integer;
{$else}
  WrongPersistentD: MissingType;
{$endif}
{$ifopt O+}
  PersistentO: Integer;
{$else}
  WrongPersistentO: MissingType;
{$endif}
{$ifndef RemovedInside}
  DefineRemovalPersists: Integer;
{$else}
  WrongRemovedDefine: MissingType;
{$endif}
{$ifdef AddedInside}
  DefineAdditionPersists: Integer;
{$else}
  WrongAddedDefine: MissingType;
{$endif}

{$ifdef Never}
  {$push}
  {$Q-}
  {$pop}
{$endif}
{$ifopt Q+}
  InactiveBranchIgnored: Integer;
{$else}
  WrongInactiveBranch: MissingType;
{$endif}

{$push}
{$Q-}
{$include directive_push_pop_pop.inc}
{$ifopt Q+}
  CrossIncludePop: Integer;
{$else}
  WrongCrossIncludePop: MissingType;
{$endif}

{ An unmatched PUSH is a valid save whose lifetime ends with this source. }
{$push}

begin
end.
