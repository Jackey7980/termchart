(() => {
  const root = document.documentElement;
  const saved = localStorage.getItem("termchart-docs-theme");
  if (saved === "light" || saved === "dark") {
    root.dataset.theme = saved;
  } else if (window.matchMedia("(prefers-color-scheme: dark)").matches) {
    root.dataset.theme = "dark";
  }

  const toggle = document.getElementById("theme-toggle");
  if (toggle) {
    const paint = () => {
      const dark = root.dataset.theme === "dark" ||
        (!root.dataset.theme && window.matchMedia("(prefers-color-scheme: dark)").matches);
      toggle.textContent = dark ? "☀" : "☾";
    };
    paint();
    toggle.addEventListener("click", () => {
      const dark = root.dataset.theme === "dark" ||
        (!root.dataset.theme && window.matchMedia("(prefers-color-scheme: dark)").matches);
      root.dataset.theme = dark ? "light" : "dark";
      localStorage.setItem("termchart-docs-theme", root.dataset.theme);
      paint();
    });
  }

  const filter = document.getElementById("nav-filter");
  if (filter) {
    filter.addEventListener("input", () => {
      const query = filter.value.trim().toLowerCase();
      document.querySelectorAll("[data-nav]").forEach((link) => {
        link.parentElement.classList.toggle("hidden", !link.textContent.toLowerCase().includes(query));
      });
      document.querySelectorAll(".nav-group").forEach((group) => {
        const visible = [...group.querySelectorAll("li")].some((li) => !li.classList.contains("hidden"));
        group.style.display = visible ? "" : "none";
      });
    });
  }

  document.addEventListener("click", (event) => {
    const button = event.target.closest(".copy-btn");
    if (!button) return;
    const code = button.closest(".code-block")?.querySelector("code")?.textContent ?? "";
    navigator.clipboard.writeText(code).then(() => {
      button.textContent = "已复制";
      setTimeout(() => { button.textContent = "复制"; }, 1400);
    }).catch(() => { button.textContent = "失败"; });
  });
})();
