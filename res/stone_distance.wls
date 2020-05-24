#!/usr/bin/env wolframscript
(* ::Package:: *)

(* ::Input:: *)
(*X0={x0,y0,z0};*)
(*X={x,y,z};*)
(*v={vx,vy.vz};*)
(*A={ax,ay,az};*)
(*B={bx,by,bz};*)
(*R={a,b,c};*)
(*Len[X_]:=Sqrt[Dot[X,X]];*)
(**)
(*sol=Solve[{D[Len[Cross[X0-B,X0-A]]/Len[B-A],{X0}]==0,Dot[X0/(R R),B-A]==0},X0][[1]]*)


(* ::Input:: *)
(*(*kontrola: X0 le\[ZHacek]\[IAcute] na p\[RHacek]\[IAcute]mce*)*)
(*Solve[(A-X0/.sol)==k(A-B),k]  ;*)


(* ::Input:: *)
(*sol2=Solve[Dot[(X0-t X0/(R R))/R,(X0-t X0/(R R))/R]-1==0/.sol,t][[2]];*)


(* ::Input:: *)
(*P=X0/.sol2;*)
(*d2[ax_,ay_,az_,bx_,by_,bz_,a_,b_,c_]=Module[{ro=P/R,rd=-Normalize[P/(R R R)]},v=ro;dt= Dot[rd,v];sqt=Sqrt[dt^2-Dot[v,v]+1];Sign[-dt-sqt]*Len[(dt+sqt)R rd]]/.sol;*)


(* ::Input:: *)
(*ContourPlot[d2[x,y,-1,x,y,0,2,1,1],{x,-2.5,2.5},{y,-2.5,2.5}]*)
