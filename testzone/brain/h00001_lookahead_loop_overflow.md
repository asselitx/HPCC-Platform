# h00001: Safe Countdown Loop for uint8_t laIndex in FragmentedXmlPullParser

## Location

`esp/bindings/SOAP/xpp/fxpp/FragmentedXmlPullParser.cpp`, `next()` function (~line 219)

## Problem

`laIndex` is declared as `uint8_t` (an unsigned 8-bit integer, range 0–255). The original loop used post-decrement in the `do-while` condition:

```cpp
do
{
    la = peekDataFrame(laIndex);
    if (la)
        la->state = DataFrame::FrameIgnored;
}
while (laIndex-- != 0);
```

When `laIndex` reaches 0, the condition evaluates `0 != 0` (false) using the value before the decrement, but the post-decrement side effect still occurs as part of condition evaluation, wrapping `laIndex` from 0 to 255 (unsigned integer underflow). While the loop terminates correctly, the underflow leaves `laIndex` in an unexpected state and is a latent source of bugs if the variable is ever read after the loop.

## Fix

Replace the post-decrement in the loop condition with an explicit break when `laIndex` is 0, then decrement manually:

```cpp
// laIndex is uint8_t (unsigned); post-decrementing past 0 would wrap
// to 255. Use an explicit break at 0 to safely iterate down to index 0.
do
{
    la = peekDataFrame(laIndex);
    if (la)
        la->state = DataFrame::FrameIgnored;
    if (laIndex == 0) break;
    laIndex--;
}
while (true);
```

## Why This Is Necessary

`uint8_t` is an **unsigned** type. Unlike signed integers, there is no negative value — decrementing 0 wraps to 255 (well-defined by the C++ standard for unsigned arithmetic, but semantically unintended here). The safe idiom is to test the boundary condition **before** performing the decrement, preventing any wrap-around.

## Safe Idiom Summary

For any countdown loop over an unsigned integer type that must include index 0:

```cpp
// Safe countdown for unsigned T:
do
{
    /* use index */
    if (index == 0) break;  // stop before underflow
    index--;
} while (true);
```

This pattern makes the termination condition explicit and avoids relying on post-decrement semantics across a zero boundary.
