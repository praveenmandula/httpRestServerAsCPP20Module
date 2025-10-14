const API_URL = `${window.location.protocol}//${window.location.host}/api/users`;

let currentPage = 1;
const PAGE_SIZE = 10;
let totalUsers = 0;

window.addEventListener("DOMContentLoaded", loadUsers);

async function loadUsers() {
    try {
        const res = await fetch(`${API_URL}?page=${currentPage}&limit=${PAGE_SIZE}`);
        const data = await res.json();

        const users = data.users || [];
        totalUsers = data.total || 0;

        renderUsers(users);
        renderPagination();
    } catch (err) {
        showToast("Error loading users: " + err.message, "error");
    }
}

function renderUsers(users) {
    const tbody = document.querySelector("#usersTable tbody");
    tbody.innerHTML = "";

    if (users.length === 0) {
        const row = document.createElement("tr");
        row.innerHTML = `<td colspan="4" style="text-align:center;">No users found</td>`;
        tbody.appendChild(row);
        return;
    }

    users.forEach(user => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${user.id}</td>
            <td>${user.name}</td>
            <td>${user.email}</td>
            <td class="actions">
                <button onclick="editUser(${user.id}, '${user.name}', '${user.email}')">Edit</button>
                <button class="delete" onclick="deleteUser(${user.id})">Delete</button>
            </td>
        `;
        tbody.appendChild(row);
    });
}

function renderPagination() {
    const totalPages = Math.ceil(totalUsers / PAGE_SIZE);
    const paginationDiv = document.getElementById("pagination");

    paginationDiv.innerHTML = "";

    const prevBtn = document.createElement("button");
    prevBtn.textContent = "⬅ Prev";
    prevBtn.disabled = currentPage === 1;
    prevBtn.onclick = () => changePage(-1);

    const nextBtn = document.createElement("button");
    nextBtn.textContent = "Next ➡";
    nextBtn.disabled = currentPage === totalPages;
    nextBtn.onclick = () => changePage(1);

    const info = document.createElement("span");
    info.textContent = `Page ${currentPage} of ${totalPages} (${totalUsers} users)`;

    paginationDiv.appendChild(prevBtn);
    paginationDiv.appendChild(info);
    paginationDiv.appendChild(nextBtn);
}

function changePage(direction) {
    const totalPages = Math.ceil(totalUsers / PAGE_SIZE);
    currentPage += direction;

    if (currentPage < 1) currentPage = 1;
    if (currentPage > totalPages) currentPage = totalPages;

    loadUsers();
}

document.getElementById("userForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    const id = document.getElementById("userId").value;
    const name = document.getElementById("name").value;
    const email = document.getElementById("email").value;

    try {
        const options = {
            method: id ? "PUT" : "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name, email }),
        };

        await fetch(id ? `${API_URL}/${id}` : API_URL, options);
        resetForm();
        await loadUsers();
        showToast(id ? "User updated successfully" : "User created successfully", "success");
    } catch (err) {
        showToast("Error saving user: " + err.message, "error");
    }
});

function editUser(id, name, email) {
    document.getElementById("userId").value = id;
    document.getElementById("name").value = name;
    document.getElementById("email").value = email;
}

async function deleteUser(id) {
    if (!confirm("Delete this user?")) return;
    try {
        await fetch(`${API_URL}/${id}`, { method: "DELETE" });
        await loadUsers();
        showToast("User deleted successfully", "success");
    } catch (err) {
        showToast("Error deleting user: " + err.message, "error");
    }
}

function resetForm() {
    document.getElementById("userId").value = "";
    document.getElementById("name").value = "";
    document.getElementById("email").value = "";
}

// ✅ Toast Notification
function showToast(message, type = "info") {
    const toast = document.getElementById("toast");
    toast.textContent = message;
    toast.className = `toast show ${type}`;

    setTimeout(() => {
        toast.className = "toast";
    }, 3000);
}
