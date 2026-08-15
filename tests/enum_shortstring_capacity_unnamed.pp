program EnumShortStringCapacityUnnamed;

type
  TCapacity = (Five := 5, Seven := 7);

const
  { An unnamed enum ordinal has the same non-integer Pascal domain as a named
    enum member, regardless of its Integer-shaped folded representation. }
  UnnamedCapacity = TCapacity(6);

type
  TBadString = string[UnnamedCapacity];

begin
end.
