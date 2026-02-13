import { SignJWT, jwtVerify } from "jose";
import { cookies } from "next/headers";

const JWT_SECRET = new TextEncoder().encode(
    process.env.JWT_SECRET || "plumbr-dev-secret-change-in-production"
);

export interface SessionPayload {
    userId: string;
    email: string;
}

export async function createSession(userId: string, email: string): Promise<string> {
    const token = await new SignJWT({ userId, email })
        .setProtectedHeader({ alg: "HS256" })
        .setIssuedAt()
        .setExpirationTime("7d")
        .sign(JWT_SECRET);

    return token;
}

export async function verifySession(): Promise<SessionPayload | null> {
    try {
        const cookieStore = await cookies();
        const token = cookieStore.get("session")?.value;
        if (!token) return null;

        const { payload } = await jwtVerify(token, JWT_SECRET);
        return payload as unknown as SessionPayload;
    } catch {
        return null;
    }
}
