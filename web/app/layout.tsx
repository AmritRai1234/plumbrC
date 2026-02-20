import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "PlumbrC — High-Performance Log Redaction",
  description:
    "Redact secrets from your logs at 5M+ lines/sec. AWS keys, passwords, emails, tokens — caught and replaced instantly. Open source, pure C, zero-allocation hot path.",
  keywords: [
    "log redaction",
    "secret scanning",
    "PII removal",
    "log security",
    "compliance",
    "HIPAA",
    "PCI-DSS",
  ],
  openGraph: {
    title: "PlumbrC — High-Performance Log Redaction",
    description: "Redact secrets from your logs at 5M+ lines/sec.",
    type: "website",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" suppressHydrationWarning>
      <head>
        <link
          href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&family=JetBrains+Mono:wght@400;500;600&display=swap"
          rel="stylesheet"
        />
      </head>
      <body className="antialiased">{children}</body>
    </html>
  );
}
