<?xml version='1.0' encoding='utf-8'?>

<!--
// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
-->

<xsl:stylesheet
	xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'
	xmlns='http://www.w3.org/1999/xhtml'
>
	<xsl:template match='/'>
		<html>
			<style>
				html, body {
					font-family: courier;
					background-color: white;
					color: black;
					font-size: 10pt;
				}
				a:link {
					decoration: none;
				}

				tr:n0 {
					background-color: #ffc;
				}
				tr:n1 {
					background-color: #ccf;
				}
				tr:hover {
					background-color: #ddb;
				}

				tr.head {
					font-weight: bold;
				}
				tr.head:hover {
					background-color: white;
				}

				td {
					vertical-align: top;
				}
				td.name {
					padding-right: 1em;
				}
				td.size {
					padding-right: 1em;
					text-align: right;
				}
				td.mtime {
					padding-right: 1em;
				}
			</style>
			<body>
				<xsl:apply-templates/>
			</body>
		</html>
	</xsl:template>

	<xsl:template match='dirlisting'>
		<h2>Index of: <xsl:value-of select='@path'/></h2>
		<table class='dirlisting'>
			<!-- header -->
			<tr class='head'>
				<td class='head name'>filename:</td>
				<td class='head size'>file size:</td>
				<td class='head mtime'>last modified:</td>
				<td class='head mimetype'>mimetype:</td>
			</tr>

			<!-- parent link -->
			<xsl:if test='not(@path = "/")'>
				<tr>
					<td class='name'>
						<a href='../'>
							parent directory
						</a>
					</td>
					<td colspan='3'>
					</td>
				</tr>
			</xsl:if>

			<!-- directory items -->
			<xsl:for-each select='*[substring(@name, string-length(@name), 1) = "/"]'>
				<xsl:sort select='@name'/>
				<tr class='n{position() div 2}'>
					<td class='name'>
						<a href='{@name}'>
							<xsl:value-of select='@name'/>
						</a>
					</td>
					<td class='size'>
						<xsl:value-of select='@size'/>
					</td>
					<td class='mtime'><xsl:value-of select='@mtime'/></td>
					<td class='mimetype'></td>
				</tr>
			</xsl:for-each>

			<!-- non-directory items -->
			<xsl:for-each select='*[not(substring(@name, string-length(@name), 1) = "/")]'>
				<xsl:sort select='@name'/>
				<tr class='n{position() div 2}'>
					<td class='name'>
						<a href='{@name}'>
							<xsl:value-of select='@name'/>
						</a>
					</td>
					<td class='size'>
						<xsl:choose>
							<xsl:when test='@size &gt;= (1024 * 1024)'>
								<xsl:value-of select='round(@size div (1024 * 1024))'/> MB
							</xsl:when>
							<xsl:when test='@size &gt;= 1024'>
								<xsl:value-of select='round(@size div 1024)'/> KB
							</xsl:when>
							<xsl:otherwise>
								<xsl:value-of select='@size'/> B
							</xsl:otherwise>
						</xsl:choose>
					</td>
					<td class='mtime'><xsl:value-of select='@mtime'/></td>
					<td class='mimetype'><xsl:value-of select='@mimetype'/></td>
				</tr>
			</xsl:for-each>
		</table>

		<hr size='1'/>
		<xsl:value-of select='@tag'/>
	</xsl:template>
</xsl:stylesheet>
