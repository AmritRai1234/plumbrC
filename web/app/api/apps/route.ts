import { NextRequest, NextResponse } from "next/server";
import { getDb, generateApiKey, generateApiSecret } from "@/app/lib/db";
import { verifySession } from "@/app/lib/auth";
import { randomUUID } from "crypto";

/* ── List user's apps ── */
export async function GET() {
    const session = await verifySession();
    if (!session) {
        return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
    }

    const db = getDb();
    const apps = db.prepare(
        "SELECT id, name, description, api_key, status, rate_limit, created_at FROM apps WHERE user_id = ? ORDER BY created_at DESC"
    ).all(session.userId);

    return NextResponse.json({ apps });
}

/* ── Create app / Delete app ── */
export async function POST(req: NextRequest) {
    const session = await verifySession();
    if (!session) {
        return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
    }

    try {
        const { action, ...data } = await req.json();

        if (action === "create") {
            return handleCreate(session.userId, data);
        } else if (action === "delete") {
            return handleDelete(session.userId, data);
        } else if (action === "regenerate") {
            return handleRegenerate(session.userId, data);
        }

        return NextResponse.json({ error: "Invalid action" }, { status: 400 });
    } catch (e: any) {
        return NextResponse.json({ error: e.message }, { status: 500 });
    }
}

function handleCreate(userId: string, { name, description }: any) {
    if (!name) {
        return NextResponse.json({ error: "App name required" }, { status: 400 });
    }

    const db = getDb();

    // Limit to 5 apps per user
    const count = db.prepare("SELECT COUNT(*) as count FROM apps WHERE user_id = ?").get(userId) as any;
    if (count.count >= 5) {
        return NextResponse.json({ error: "Maximum 5 apps per account" }, { status: 400 });
    }

    const id = randomUUID();
    const apiKey = generateApiKey();
    const apiSecret = generateApiSecret();

    db.prepare(
        "INSERT INTO apps (id, user_id, name, description, api_key, api_secret) VALUES (?, ?, ?, ?, ?, ?)"
    ).run(id, userId, name, description || "", apiKey, apiSecret);

    return NextResponse.json({
        app: { id, name, description, api_key: apiKey, api_secret: apiSecret, status: "active" },
    });
}

function handleDelete(userId: string, { appId }: any) {
    const db = getDb();
    const app = db.prepare("SELECT * FROM apps WHERE id = ? AND user_id = ?").get(appId, userId);
    if (!app) {
        return NextResponse.json({ error: "App not found" }, { status: 404 });
    }

    db.prepare("DELETE FROM api_usage WHERE app_id = ?").run(appId);
    db.prepare("DELETE FROM apps WHERE id = ?").run(appId);

    return NextResponse.json({ success: true });
}

function handleRegenerate(userId: string, { appId }: any) {
    const db = getDb();
    const app = db.prepare("SELECT * FROM apps WHERE id = ? AND user_id = ?").get(appId, userId);
    if (!app) {
        return NextResponse.json({ error: "App not found" }, { status: 404 });
    }

    const newKey = generateApiKey();
    const newSecret = generateApiSecret();

    db.prepare("UPDATE apps SET api_key = ?, api_secret = ? WHERE id = ?")
        .run(newKey, newSecret, appId);

    return NextResponse.json({ api_key: newKey, api_secret: newSecret });
}
