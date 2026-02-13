import { NextRequest, NextResponse } from "next/server";
import { getDb, generateApiKey, generateApiSecret } from "@/app/lib/db";
import { createSession, verifySession } from "@/app/lib/auth";
import bcrypt from "bcryptjs";
import { randomUUID } from "crypto";

/* ── Sign Up ── */
export async function POST(req: NextRequest) {
    try {
        const { action, ...data } = await req.json();

        if (action === "signup") {
            return handleSignup(data);
        } else if (action === "login") {
            return handleLogin(data);
        } else if (action === "logout") {
            return handleLogout();
        }

        return NextResponse.json({ error: "Invalid action" }, { status: 400 });
    } catch (e: any) {
        return NextResponse.json({ error: e.message }, { status: 500 });
    }
}

async function handleSignup({ email, password, name }: any) {
    if (!email || !password) {
        return NextResponse.json({ error: "Email and password required" }, { status: 400 });
    }

    const db = getDb();

    // Check existing
    const existing = db.prepare("SELECT id FROM users WHERE email = ?").get(email);
    if (existing) {
        return NextResponse.json({ error: "Email already registered" }, { status: 409 });
    }

    const hashedPassword = await bcrypt.hash(password, 10);
    const id = randomUUID();

    db.prepare("INSERT INTO users (id, email, password, name) VALUES (?, ?, ?, ?)")
        .run(id, email, hashedPassword, name || email.split("@")[0]);

    const token = await createSession(id, email);

    const response = NextResponse.json({ success: true, userId: id });
    response.cookies.set("session", token, {
        httpOnly: true,
        secure: process.env.NODE_ENV === "production",
        sameSite: "lax",
        maxAge: 60 * 60 * 24 * 7, // 7 days
        path: "/",
    });

    return response;
}

async function handleLogin({ email, password }: any) {
    if (!email || !password) {
        return NextResponse.json({ error: "Email and password required" }, { status: 400 });
    }

    const db = getDb();
    const user = db.prepare("SELECT * FROM users WHERE email = ?").get(email) as any;

    if (!user) {
        return NextResponse.json({ error: "Invalid credentials" }, { status: 401 });
    }

    const valid = await bcrypt.compare(password, user.password);
    if (!valid) {
        return NextResponse.json({ error: "Invalid credentials" }, { status: 401 });
    }

    const token = await createSession(user.id, user.email);

    const response = NextResponse.json({ success: true, userId: user.id });
    response.cookies.set("session", token, {
        httpOnly: true,
        secure: process.env.NODE_ENV === "production",
        sameSite: "lax",
        maxAge: 60 * 60 * 24 * 7,
        path: "/",
    });

    return response;
}

async function handleLogout() {
    const response = NextResponse.json({ success: true });
    response.cookies.set("session", "", {
        httpOnly: true,
        maxAge: 0,
        path: "/",
    });
    return response;
}

/* ── Get Session ── */
export async function GET() {
    const session = await verifySession();
    if (!session) {
        return NextResponse.json({ authenticated: false }, { status: 401 });
    }

    const db = getDb();
    const user = db.prepare("SELECT id, email, name, created_at FROM users WHERE id = ?").get(session.userId) as any;

    return NextResponse.json({ authenticated: true, user });
}
