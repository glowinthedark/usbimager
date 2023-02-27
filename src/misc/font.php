#!/usr/bin/php
<?php
/*
 * usbimager/misc/font.php
 *
 * Copyright (C) 2023 bzt (bztsrc@gitlab)
 *
 * @brief small tool to generate unifont.h
 */

// you can download the latest GNU Unifont from https://unifoundry.com/unifont
$font = "unifont-15.0.01.bdf.gz";

if(!file_exists("unifont.sfn")) {
    echo("Checking multibyte characters to get UNICODE ranges...");
    // you can download the latest Blocks.txt from http://www.unicode.org/Public/UCD/latest/ucd/Blocks.txt
    $b = file("Blocks.txt"); $blocks = []; $r = "";
    foreach($b as $v) if(strlen($v) > 10 && $v[4] == '.' && $v[10] == ';') $blocks[] = [ hexdec(substr($v, 0, 4)), hexdec(substr($v, 6, 4)), 0 ];
    // collect UNICODE ranges that we actually use
    $data = file_get_contents("../lang.c");
    for($i = 0; $i < mb_strlen($data); $i++) {
        $v = mb_ord(mb_substr($data, $i, 1));
        for($j = 0; $j < count($blocks); $j++) if($v >= $blocks[$j][0] && $v <= $blocks[$j][1]) $blocks[$j][2]++;
    }
    foreach($blocks as $b)
        if($b[2]) $r .= " -r 0x".dechex($b[0] < 32 ? 32 : ($b[0] == 0x80 ? 0xA0 : $b[0]))." 0x".dechex($b[1] == 0x7F ? 0x7E : $b[1])." tmp.sfn";
    echo("OK\n");
    // convert selected BDF glyphs to SFN, see https://gitlab.com/bztsrc/scalable-font2
    system("./sfnconv ".$font." tmp.sfn");
    system("./sfnconv -S code -p -r 0 0 tmp.sfn ".$r." -r 0x3130 0x318F tmp.sfn -r 0x31F0 0x31FF tmp.sfn unifont.sfn");
    unlink("tmp.sfn");
}
// save as a C header file
$data = file_get_contents("unifont.sfn");
$s = "#define UNIFONT_SIZE ".strlen(gzdecode($data))."\n";
$s.= "unsigned char unifont[".(strlen($data) - 10)."] = { ";
for($i = 10; $i < strlen($data); $i++) $s.=($i > 10 ? "," :"").ord($data[$i]);
$s.= " };\r\n";
file_put_contents("unifont.h", $s);
