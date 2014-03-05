/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/fxtf/web-animations/#idl-def-Animation
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

//typedef (Element or PseudoElementReference) AnimationTarget;
typedef Element AnimationTarget;

/*[Constructor (AnimationTarget? element,
              (AnimationEffect or CustomEffect or OneOrMoreKeyframes)? effect,
              optional (unrestricted double or TimingInput)? timing = null)]
*/
interface Animation /*: TimedItem */{
//             attribute (AnimationEffect or CustomEffect)? effect;
    readonly attribute AnimationTarget?                   target;
    [NewObject]
    Animation clone ();
};
